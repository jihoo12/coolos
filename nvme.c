#include "nvme.h"
#include "graphics.h"
#include "heap.h"
#include "libc.h"
#include "memory.h"
#include "timer.h"

// Define a simple wait helper (primitive delay)
static void SleepStub(int count) {
  for (volatile int i = 0; i < count * 1000; i++)
    ;
}

// Global context (single controller support for now)
static NVMe_Context g_nvme_ctx;
static uint8_t g_admin_sq_buffer[4096] __attribute__((aligned(4096)));
static uint8_t g_admin_cq_buffer[4096] __attribute__((aligned(4096)));
static uint8_t g_identify_buffer[4096] __attribute__((aligned(4096)));

// Buffers for IO Queues (allocated dynamically or static)
// Using static for now to ensure alignment easily
static uint8_t g_io_sq_buffer[4096] __attribute__((aligned(4096)));
static uint8_t g_io_cq_buffer[4096] __attribute__((aligned(4096)));

void NVMe_SubmitCommand(NVMe_Queue *q, NVMe_SQEntry *cmd) {
  // Copy command to SQ slot
  NVMe_SQEntry *slot = &q->SQ_Base[q->Tail];
  memcpy(slot, cmd, sizeof(NVMe_SQEntry));

  // Increment Tail
  q->Tail++;
  if (q->Tail >= q->Size) {
    q->Tail = 0;
  }

  // Write Tail Doorbell
  *q->DoorbellTail = q->Tail;
}

void NVMe_WaitForCompletion(NVMe_Queue *q, uint16_t cid) {
  while (1) {
    NVMe_CQEntry *entry = &q->CQ_Base[q->Head];

    // Check Phase Tag
    // The Phase Tag (P) bit in status field flips every pass through the queue
    if ((entry->Status & 0x1) == q->Phase) {
      // Entry is new

      // Check Command ID
      if (entry->CommandID == cid) {
        // Handled
        // Update Head
        q->Head++;
        if (q->Head >= q->Size) {
          q->Head = 0;
          q->Phase = !q->Phase; // Flip expected phase
        }

        // Ring Head Doorbell
        *q->DoorbellHead = q->Head;
        return;
      } else {
        // Consume other completions
        q->Head++;
        if (q->Head >= q->Size) {
          q->Head = 0;
          q->Phase = !q->Phase;
        }
        *q->DoorbellHead = q->Head;
      }
    }
  }
}

void NVMe_SetupIOQueues(NVMe_Context *ctx) {
  NVMe_SQEntry cmd;

  // 1. Create IO Completion Queue
  // Opcode = 0x05
  // PRP1 = Queue Base Address
  // CDW10 = (Queue Size - 1) << 16 | QID
  // CDW11 = Interrupt Vector | (1 << 0) (Physically Contiguous)
  memset(&cmd, 0, sizeof(cmd));
  cmd.Opcode = NVME_ADMIN_OP_CREATE_IOCQ;
  cmd.CommandID = 2;
  cmd.Prp1 = (uint64_t)(uintptr_t)g_io_cq_buffer;
  cmd.Cdw10 = ((64 - 1) << 16) | 1; // Size 64, QID 1
  cmd.Cdw11 = 1;                    // Phys Contiguous

  NVMe_SubmitCommand(&ctx->AdminQueue, &cmd);
  NVMe_WaitForCompletion(&ctx->AdminQueue, 2);

  // 2. Create IO Submission Queue
  // Opcode = 0x01
  // PRP1 = Queue Base Address
  // CDW10 = (Queue Size - 1) << 16 | QID
  // CDW11 = (CQID << 16) | (1 << 0) (Phys Contiguous)
  memset(&cmd, 0, sizeof(cmd));
  cmd.Opcode = NVME_ADMIN_OP_CREATE_IOSQ;
  cmd.CommandID = 3;
  cmd.Prp1 = (uint64_t)(uintptr_t)g_io_sq_buffer;
  cmd.Cdw10 = ((64 - 1) << 16) | 1; // Size 64, QID 1
  cmd.Cdw11 = (1 << 16) | 1;        // CQID 1, Phys Contiguous

  NVMe_SubmitCommand(&ctx->AdminQueue, &cmd);
  NVMe_WaitForCompletion(&ctx->AdminQueue, 3);

  // Setup Local Queue Struct
  ctx->IOQueue.ID = 1;
  ctx->IOQueue.Head = 0;
  ctx->IOQueue.Tail = 0;
  ctx->IOQueue.Size = 64;
  ctx->IOQueue.Phase = 1;
  ctx->IOQueue.SQ_Base = (NVMe_SQEntry *)g_io_sq_buffer;
  ctx->IOQueue.CQ_Base = (NVMe_CQEntry *)g_io_cq_buffer;

  // Doorbell for QID 1
  // Tail = 0x1000 + (2 * 1 * 4) = 0x1000 + 8 = 0x1008
  uintptr_t db_base = (uintptr_t)ctx->Regs + 0x1000;
  ctx->IOQueue.DoorbellTail = (uint32_t *)(db_base + 8);
  ctx->IOQueue.DoorbellHead = (uint32_t *)(db_base + 12);
}

void NVMe_IdentifyController(NVMe_Context *ctx) {
  NVMe_SQEntry cmd;
  memset(&cmd, 0, sizeof(cmd));

  cmd.Opcode = NVME_ADMIN_OP_IDENTIFY;
  cmd.CommandID = 1;
  cmd.Prp1 = (uint64_t)(uintptr_t)g_identify_buffer;
  cmd.Cdw10 = 1; // CNS = 1 (Identify Controller)

  NVMe_SubmitCommand(&ctx->AdminQueue, &cmd);
  NVMe_WaitForCompletion(&ctx->AdminQueue, 1);

  // Parse Identify Controller Data Structure (Figure 247 in NVMe spec 1.4)
  // Model Number is at byte 24, length 40
  char model[41];
  memcpy(model, g_identify_buffer + 24, 40);
  model[40] = 0;

  // Trim spaces
  for (int i = 39; i >= 0; i--) {
    if (model[i] == ' ')
      model[i] = 0;
    else
      break;
  }

  char msg[100];
  // "NVMe Model: <model>"
  char *prefix = "NVME MODEL: ";
  int p_len = 12; // "NVMe Model: " length
  memcpy(msg, prefix, p_len);
  int m_len = 0;
  while (model[m_len] != 0)
    m_len++;
  memcpy(msg + p_len, model, m_len);
  msg[p_len + m_len] = 0;

  Graphics_Print(100, 640, msg, 0xCB4B16);
}

void NVMe_IdentifyNamespace(NVMe_Context *ctx) {
  // Identify Active Namespaces (CNS = 2) or just use NSID 1 blindly for qemu
  ctx->NSID = 1;
  Graphics_Print(100, 660, "NVME: DEFAULT NSID 1 SELECTED", 0x859900);
}

void NVMe_Init(PCI_Device *device) {
  if (!device)
    return;

  g_nvme_ctx.PciDev = device;

  // 1. Map BAR0 to get registers
  uint64_t bar = device->BaseAddress[0] & 0xFFFFFFF0;
  if (device->BaseAddress[1] != 0) {
    bar |= ((uint64_t)device->BaseAddress[1] << 32);
  }

  // NOTE: In a real OS, you must ioremap this.
  // Assuming identity mapping for now since CoolOS uses identity mapping
  // mostly. BUT we must ensure high MMIO addresses are mapped in our page
  // table.
  Memory_MapMMIO((void *)bar, 0x4000); // Map 16KB
  g_nvme_ctx.Regs = (NVMe_Registers *)bar;

  NVMe_Registers *regs = g_nvme_ctx.Regs;

  // 2. Disable Controller (CC.EN = 0)
  if (regs->Cc & 0x1) {
    regs->Cc &= ~0x1;
  }

  // Wait for CSTS.RDY to become 0
  while (regs->Csts & 0x1) {
    SleepStub(1);
  }

  // 3. Configure Admin Queue
  uint32_t q_size = 64;
  regs->Aqa = ((q_size - 1) << 16) | (q_size - 1);

  // Set ASQ and ACQ addresses
  memset(g_admin_sq_buffer, 0, 4096);
  memset(g_admin_cq_buffer, 0, 4096);

  regs->Asq = (uint64_t)(uintptr_t)g_admin_sq_buffer;
  regs->Acq = (uint64_t)(uintptr_t)g_admin_cq_buffer;

  // 4. Setup Internal Queue Struct
  g_nvme_ctx.AdminQueue.ID = 0;
  g_nvme_ctx.AdminQueue.Head = 0;
  g_nvme_ctx.AdminQueue.Tail = 0;
  g_nvme_ctx.AdminQueue.Size = q_size;
  g_nvme_ctx.AdminQueue.Phase = 1;
  g_nvme_ctx.AdminQueue.SQ_Base = (NVMe_SQEntry *)g_admin_sq_buffer;
  g_nvme_ctx.AdminQueue.CQ_Base = (NVMe_CQEntry *)g_admin_cq_buffer;

  // Doorbell registers
  uintptr_t db_base = (uintptr_t)regs + 0x1000;
  g_nvme_ctx.AdminQueue.DoorbellTail = (uint32_t *)db_base;
  g_nvme_ctx.AdminQueue.DoorbellHead = (uint32_t *)(db_base + 4);

  // 5. Enable Controller
  uint32_t cc = 0;
  cc |= (4 << 20); // IOCQES = 4 (16 bytes = 2^4)
  cc |= (6 << 16); // IOSQES = 6 (64 bytes = 2^6)
  cc |= 1;         // EN = 1
  regs->Cc = cc;

  // Wait for CSTS.RDY to become 1
  while (!(regs->Csts & 0x1)) {
    SleepStub(1);
  }

  // 6. Identify Controller
  NVMe_IdentifyController(&g_nvme_ctx);

  // 7. Setup IO Queues
  NVMe_SetupIOQueues(&g_nvme_ctx);

  // 8. Identify Namespace (Select default)
  NVMe_IdentifyNamespace(&g_nvme_ctx);
}

int NVMe_Read(uint32_t nsid, uint64_t lba, void *buffer, uint32_t count) {
  NVMe_SQEntry cmd;
  memset(&cmd, 0, sizeof(cmd));

  // NVMe Read Opcode = 0x02
  cmd.Opcode = NVME_OP_READ;
  cmd.CommandID = 100; // Arbitrary
  cmd.NSID = nsid;

  // PRP 1
  cmd.Prp1 = (uint64_t)(uintptr_t)buffer;

  // CDW10: Starting LBA Low
  cmd.Cdw10 = (uint32_t)lba;

  // CDW11: Starting LBA High
  cmd.Cdw11 = (uint32_t)(lba >> 32);

  // CDW12: Number of Logical Blocks (0's based). So count-1.
  cmd.Cdw12 = (count - 1) & 0xFFFF;

  NVMe_SubmitCommand(&g_nvme_ctx.IOQueue, &cmd);
  NVMe_WaitForCompletion(&g_nvme_ctx.IOQueue, 100);

  return 0; // Success
}
