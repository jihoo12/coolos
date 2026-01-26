#ifndef NVME_H
#define NVME_H

#include "pci.h"
#include <stdint.h>

// NVMe Controller Registers (Offset from BAR0)
typedef struct {
  uint64_t Cap;   // Controller Capabilities
  uint32_t Vs;    // Version
  uint32_t Intms; // Interrupt Mask Set
  uint32_t Intmc; // Interrupt Mask Clear
  uint32_t Cc;    // Controller Configuration
  uint32_t Reserved1;
  uint32_t Csts;   // Controller Status
  uint32_t Nssr;   // NVM Subsystem Reset (Optional)
  uint32_t Aqa;    // Admin Queue Attributes
  uint64_t Asq;    // Admin Submission Queue Base Address
  uint64_t Acq;    // Admin Completion Queue Base Address
  uint32_t Cmbloc; // Controller Memory Buffer Location (Optional)
  uint32_t Cmbsz;  // Controller Memory Buffer Size (Optional)
  uint32_t Bpinfo; // Boot Partition Information
  uint32_t Bprsel; // Boot Partition Read Select
  uint64_t Bpmbl;  // Boot Partition Memory Buffer Location
  uint64_t Cmbmsc; // Controller Memory Buffer Memory Space Control
  uint32_t Cmbsts; // Controller Memory Buffer Status
                   // ... padding to 0x1000 usually
} volatile NVMe_Registers;

// NVMe Submission Queue Entry (Command) - 64 bytes
typedef struct {
  uint8_t Opcode;
  uint8_t Flags;
  uint16_t CommandID;
  uint32_t NSID;
  uint64_t Reserved1;
  uint64_t MetadataPtr;
  uint64_t Prp1;
  uint64_t Prp2;
  uint32_t Cdw10;
  uint32_t Cdw11;
  uint32_t Cdw12;
  uint32_t Cdw13;
  uint32_t Cdw14;
  uint32_t Cdw15;
} __attribute__((packed)) NVMe_SQEntry;

// NVMe Completion Queue Entry - 16 bytes
typedef struct {
  uint32_t Cdw0;
  uint32_t Reserved;
  uint16_t SqHead; // Pointer to current SQ Head pointer updated by controller
  uint16_t SqId;   // SQ Identifier
  uint16_t CommandID;
  uint16_t Status; // Phase Tag (P) is Bit 0 set by controller to flip byte
} __attribute__((packed)) NVMe_CQEntry;

// NVMe Admin Opcodes
#define NVME_ADMIN_OP_DELETE_IOSQ 0x00
#define NVME_ADMIN_OP_CREATE_IOSQ 0x01
#define NVME_ADMIN_OP_GET_LOG_PAGE 0x02
#define NVME_ADMIN_OP_DELETE_IOCQ 0x04
#define NVME_ADMIN_OP_CREATE_IOCQ 0x05
#define NVME_ADMIN_OP_IDENTIFY 0x06
#define NVME_ADMIN_OP_ABORT 0x08
#define NVME_ADMIN_OP_SET_FEATURES 0x09
#define NVME_ADMIN_OP_GET_FEATURES 0x0A
#define NVME_ADMIN_OP_ASYNC_EVENT_REQ 0x0C
#define NVME_ADMIN_OP_NS_MGMT 0x0D
#define NVME_ADMIN_OP_FW_COMMIT 0x10
#define NVME_ADMIN_OP_FW_IMAGE_DL 0x11
#define NVME_ADMIN_OP_DEV_SELF_TEST 0x14
#define NVME_ADMIN_OP_NS_ATTACH 0x15
#define NVME_ADMIN_OP_KEEP_ALIVE 0x18
#define NVME_ADMIN_OP_DIRECTIVE_SEND 0x19
#define NVME_ADMIN_OP_DIRECTIVE_RECV 0x1A
#define NVME_ADMIN_OP_VIRT_MGMT 0x1C
#define NVME_ADMIN_OP_NVME_MI_SEND 0x1D
#define NVME_ADMIN_OP_NVME_MI_RECV 0x1E
#define NVME_ADMIN_OP_DOORBELL_BUF_OL 0x7C

// NVMe NVM Opcodes
#define NVME_OP_READ 0x02
#define NVME_OP_WRITE 0x01

// Internal Queue Structure
typedef struct {
  uint16_t ID;
  uint16_t Tail; // Submission Tail (Host writes here)
  uint16_t Head; // Completion Head (Host reads here)
  uint16_t Size;
  uint16_t Phase;         // Completion Queue Phase Bit (to detect new entries)
  uint32_t *DoorbellTail; // Pointer to Submission Queue Tail Doorbell
  uint32_t *DoorbellHead; // Pointer to Completion Queue Head Doorbell
  NVMe_SQEntry *SQ_Base;
  NVMe_CQEntry *CQ_Base;
} NVMe_Queue;

// NVMe Context
typedef struct {
  PCI_Device *PciDev;
  NVMe_Registers *Regs;
  NVMe_Queue AdminQueue;
  NVMe_Queue IOQueue; // Simple single IO Queue
  uint32_t NSID;      // Active Namespace ID
} NVMe_Context;

// Functions
void NVMe_Init(PCI_Device *device);
void NVMe_IdentifyController(NVMe_Context *ctx);
int NVMe_Read(uint32_t nsid, uint64_t lba, void *buffer, uint32_t count);
int NVMe_Write(uint32_t nsid, uint64_t lba, void *buffer, uint32_t count);

#endif
