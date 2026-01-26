#include "acpi.h"
#include "apic.h"
#include "efi.h"
#include "gdt.h"
#include "graphics.h"
#include "interrupt.h"
#include "ioapic.h"
#include "keyboard.h"
#include "libc.h"
#include "memory.h"
#include "nvme.h"
#include "pci.h"
#include "schedule.h"
#include "syscall.h" // Added include
#include "timer.h"
// Helper to print a hex number (very primitive)
void PrintHex(EFI_SYSTEM_TABLE *SystemTable, uint64_t val) {
  uint16_t out[19];
  out[0] = '0';
  out[1] = 'x';
  for (int i = 0; i < 16; i++) {
    uint8_t nibble = (val >> (60 - i * 4)) & 0xF;
    out[i + 2] = (nibble < 10) ? (nibble + '0') : (nibble - 10 + 'A');
  }
  out[18] = 0;
  SystemTable->ConOut->OutputString(SystemTable->ConOut, out);
}

/***
void TaskA() {
  while (1) {
    Graphics_Print(400, 100, "TASK A RUNNING ", 0x00FFFF);
  }
}
void TaskB() {
  while (1) {
    Graphics_Print(400, 130, "TASK B RUNNING ", 0xFFFF00);
  }
}
  it's not neccessary currently
**/

// ... (existing includes)

// Switch to User Mode
// entry_point: Target function execution address
// user_stack: User stack pointer (top of the stack)
void SwitchToUserMode(void *entry_point, void *user_stack) {
  // Selectors for User Mode (Ring 3)
  // USER_DATA_SEL (Index 3) = 0x18 | 3 = 0x1B
  // USER_CODE_SEL (Index 4) = 0x20 | 3 = 0x23
  asm volatile("mov $0x1B, %%ax\n" // User Data Segment
               "mov %%ax, %%ds\n"
               "mov %%ax, %%es\n"
               "mov %%ax, %%fs\n"
               "mov %%ax, %%gs\n"

               "pushq $0x1B\n" // SS
               "pushq %1\n"    // RSP
               "pushfq\n"      // RFLAGS
               "popq %%rax\n"
               "orq $0x200, %%rax\n" // Enable Interrupts (IF)
               "pushq %%rax\n"       // RFLAGS
               "pushq $0x23\n"       // CS
               "pushq %0\n"          // RIP
               "iretq\n"
               :
               : "r"(entry_point), "r"(user_stack)
               : "rax", "memory");
}

// Switch to User Mode
// ...
// usermode entry point
void UserMain() {
  char *msg = "HELLO WORLD FROM MY KERNEL!\n";
  char *msg1 = "HELLO FROM USER MODE VIA SYSCALL!\n";
  char *msg2 = "NICE TO MEET YOU!";
  // syscall(SYSCALL_PRINT, msg, color)
  // RAX = 1
  // RDI = msg
  // RSI = color
  // RCX and R11 are clobbered
  // asm volatile("mov $0, %%rax\n"
  //           "mov $0x002b36,%%rdi\n"
  //           "syscall\n"
  //           :
  //           :
  //           : "rax", "rdi", "rsi", "rcx", "r11");
  asm volatile("mov $1, %%rax\n"
               "mov %0, %%rdi\n"
               "mov $0x93a1a1, %%rsi\n"
               "syscall\n"
               :
               : "r"(msg)
               : "rax", "rdi", "rsi", "rcx", "r11");
  asm volatile("mov $1, %%rax\n"
               "mov %0, %%rdi\n"
               "mov $0x93a1a1, %%rsi\n"
               "syscall\n"
               :
               : "r"(msg1)
               : "rax", "rdi", "rsi", "rcx", "r11");
  asm volatile("mov $1, %%rax\n"
               "mov %0, %%rdi\n"
               "mov $0x93a1a1, %%rsi\n"
               "syscall\n"
               :
               : "r"(msg2)
               : "rax", "rdi", "rsi", "rcx", "r11");
  asm volatile("mov $4, %%rax\n"
               "syscall\n"
               :
               :
               : "rax");
}

// ... (KernelMain arguments) ...
void KernelMain(EFI_PHYSICAL_ADDRESS fb_base, uint32_t width, uint32_t height,
                uint32_t ppsl, UINTN map_size, EFI_MEMORY_DESCRIPTOR *map,
                UINTN map_key, UINTN desc_size, uint32_t desc_ver,
                void *image_base, uint64_t image_size, RSDP *rsdp) {

  // ... (existing init code: PageAllocator, ACPI, GDT, IDT, Graphics) ...
  PageAllocator_Init(map, map_size, desc_size);
  PageAllocator_MarkUsed(image_base, (image_size + 4095) / 4096);

  if (rsdp) {
    ACPI_Init(rsdp);
  }

  GDT_Init();
  IDT_Init();
  Syscall_Init();

  Graphics_Init(fb_base, width, height, ppsl);

  // ... (LAPIC setup) ...
  uint64_t lapic_addr = 0xFEE00000;
  if (rsdp) {
    MADT *madt = (MADT *)ACPI_FindTable("APIC");
    if (madt) {
      lapic_addr = madt->LocalApicAddress;
    }
  }

  PageTable_Init(image_base, image_size, (void *)fb_base,
                 (uint64_t)ppsl * height * 4, map, map_size, desc_size,
                 lapic_addr);

  void *heap_addr = PageAllocator_Alloc(4096);
  if (heap_addr) {
    Heap_Init(heap_addr, 4096 * 4096);
  }

  Graphics_Clear(0xEEE8D5);
  Graphics_Print(100, 100, "HELLO FROM COOLOS KERNEL!", 0x268BD2);

  // ... (Rest of existing output logic) ...

  if (rsdp) {
    // ... (ACPI/APIC init logic) ...
    MADT *madt = (MADT *)ACPI_FindTable("APIC");
    if (madt) {
      // ... (All existing APIC/Timer init logic) ...
      // Initialize LAPIC
      LAPIC_Init((void *)(uintptr_t)madt->LocalApicAddress);
      Interrupt_RegisterHandler(INT_TIMER, Timer_Handler);
      Interrupt_RegisterHandler(INT_KEYBOARD, Keyboard_Handler);
      uint32_t ticks_10ms = LAPIC_CalibrateTimer();
      LAPIC_TimerInit(ticks_10ms / 10);

      uint8_t *ptr = (uint8_t *)madt->InterruptControllers;
      uint8_t *end = (uint8_t *)madt + madt->Header.Length;
      while (ptr < end) {
        MADT_EntryHeader *h = (MADT_EntryHeader *)ptr;
        if (h->Type == MADT_TYPE_IOAPIC) {
          MADT_IoApic *io = (MADT_IoApic *)ptr;
          IOAPIC_Init((void *)(uintptr_t)io->IoApicAddress);
          IOAPIC_MapIRQ(1, INT_KEYBOARD, 0);
          break;
        }
        ptr += h->Length;
      }

      Scheduler_Init();
      asm volatile("sti");

      PCI_Init();
      PCI_Device *nvme = PCI_GetNVMeController();
      if (nvme) {
        NVMe_Init(nvme);

        // Write 1 block (LBA 0) with pattern
        uint8_t *write_buf = kmalloc(4096);
        if (write_buf) {
          char *msg = "COOLOS NVME TEST WRITE PATTERN";
          memset(write_buf, 0, 4096);
          int len = 0;
          while (msg[len])
            len++;
          memcpy(write_buf, msg, len);

          NVMe_Write(1, 0, write_buf, 1);
          Graphics_Print(100, 660, "NVME WRITE: SENT", 0x268BD2);
        }

        // Read 1 block (LBA 0)
        uint8_t *read_buf = kmalloc(4096);
        if (read_buf) {
          memset(read_buf, 0, 4096);
          NVMe_Read(1, 0, read_buf, 1);

          // Convert first 16 bytes to hex string
          char hex_msg[100];
          char *hex_chars = "0123456789ABCDEF";
          int pos = 0;

          char *prefix = "NVME READ: ";
          while (*prefix)
            hex_msg[pos++] = *prefix++;

          for (int i = 0; i < 16; i++) {
            hex_msg[pos++] = hex_chars[(read_buf[i] >> 4) & 0xF];
            hex_msg[pos++] = hex_chars[read_buf[i] & 0xF];
            hex_msg[pos++] = ' ';
          }
          hex_msg[pos] = 0;

          Graphics_Print(100, 680, hex_msg, 0xCB4B16);
        }
      } else {
        Graphics_Print(100, 600, "PCI: NO NVME FOUND", 0xDC322F);
      }

      // --- RING 3 SWITCHING ---
      Graphics_Print(100, 525, "PREPARING USER MODE...", 0x268BD2);

      // 1. Setup Kernel Stack for Interrupts (RSP0 in TSS)
      // When interrupt occurs in Ring 3, CPU switches to Ring 0 and loads RSP
      // from TSS.RSP0
      void *kernel_stack = kmalloc(4096);
      if (kernel_stack) {
        TSS_SetStack((uint64_t)kernel_stack + 4096);
      } else {
        Graphics_Print(100, 550, "KSTACK ALLOC FAIL", 0xDC322F);
        while (1)
          ;
      }

      // 2. Setup User Stack
      void *user_stack = kmalloc(4096);
      if (!user_stack) {
        Graphics_Print(100, 550, "USTACK ALLOC FAIL", 0xDC322F);
        while (1)
          ;
      }

      // 3. Switch to Ring 3
      Graphics_Print(100, 550, "SWITCHING TO RING 3...", 0x859900); // Green
      SwitchToUserMode(UserMain, (void *)((uint64_t)user_stack + 4096));

      // SwitchToUserMode(UserMain, (void *)((uint64_t)user_stack + 4096));

      // Code below should NOT be reached
      // Graphics_Print(100, 575, "UNREACHABLE CODE", 0xDC322F);
    }
  }

  // Use RSDP check or just unconditional PCI init if ACPI failed,
  // but let's do it after everything else to show it works independently
  // roughly

  while (1) {
    asm("hlt");
  }
}

// ... (EfiMain remains) ...

EFI_STATUS EFIAPI EfiMain(EFI_HANDLE ImageHandle,
                          EFI_SYSTEM_TABLE *SystemTable) {
  EFI_STATUS status;
  EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = 0;
  EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

  SystemTable->ConOut->OutputString(SystemTable->ConOut,
                                    (uint16_t *)L"Locating GOP...\r\n");
  status =
      SystemTable->BootServices->LocateProtocol(&gop_guid, 0, (void **)&gop);

  if (status != EFI_SUCCESS) {
    SystemTable->ConOut->OutputString(SystemTable->ConOut,
                                      (uint16_t *)L"LocateProtocol failed: ");
    PrintHex(SystemTable, status);
    SystemTable->ConOut->OutputString(SystemTable->ConOut, (uint16_t *)L"\r\n");

    // Let's try to locate the count of handles that support GOP
    UINTN no_handles = 0;
    EFI_HANDLE *handles = 0;
    status = SystemTable->BootServices->LocateHandleBuffer(
        2, &gop_guid, 0, &no_handles, &handles);
    if (status == EFI_SUCCESS) {
      SystemTable->ConOut->OutputString(SystemTable->ConOut,
                                        (uint16_t *)L"Found handles: ");
      PrintHex(SystemTable, no_handles);
      SystemTable->ConOut->OutputString(SystemTable->ConOut,
                                        (uint16_t *)L"\r\n");
      // If handles found, try the first one
      if (no_handles > 0) {
        status = SystemTable->BootServices->HandleProtocol(
            handles[0], &gop_guid, (void **)&gop);
        if (status == EFI_SUCCESS) {
          SystemTable->ConOut->OutputString(
              SystemTable->ConOut,
              (uint16_t *)L"HandleProtocol Succeeded!\r\n");
        }
      }
    } else {
      SystemTable->ConOut->OutputString(
          SystemTable->ConOut, (uint16_t *)L"LocateHandleBuffer failed: ");
      PrintHex(SystemTable, status);
      SystemTable->ConOut->OutputString(SystemTable->ConOut,
                                        (uint16_t *)L"\r\n");
    }
  }

  if (!gop) {
    SystemTable->ConOut->OutputString(
        SystemTable->ConOut,
        (uint16_t *)L"Critical: GOP is NULL. Hanging.\r\n");
    while (1)
      ;
  }

  // Get LoadedImageProtocol to find binary range
  EFI_LOADED_IMAGE_PROTOCOL *loaded_image;
  EFI_GUID loaded_image_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
  status = SystemTable->BootServices->HandleProtocol(
      ImageHandle, &loaded_image_guid, (void **)&loaded_image);
  if (status != EFI_SUCCESS) {
    SystemTable->ConOut->OutputString(
        SystemTable->ConOut,
        (uint16_t *)L"Failed to get LoadedImageProtocol\r\n");
    while (1)
      ;
  }

  // Find RSDP from ConfigurationTable
  RSDP *rsdp = NULL;
  EFI_GUID acpi_20_guid = ACPI_20_TABLE_GUID;
  for (UINTN i = 0; i < SystemTable->NumberOfTableEntries; i++) {
    EFI_CONFIGURATION_TABLE *table =
        &((EFI_CONFIGURATION_TABLE *)SystemTable->ConfigurationTable)[i];
    if (memcmp(&table->VendorGuid, &acpi_20_guid, sizeof(EFI_GUID)) == 0) {
      rsdp = (RSDP *)table->VendorTable;
      break;
    }
  }

  if (rsdp) {
    SystemTable->ConOut->OutputString(SystemTable->ConOut,
                                      (uint16_t *)L"RSDP found!\r\n");
  } else {
    SystemTable->ConOut->OutputString(SystemTable->ConOut,
                                      (uint16_t *)L"RSDP NOT found!\r\n");
  }

  // Prepare for ExitBootServices
  UINTN map_size = 0;
  EFI_MEMORY_DESCRIPTOR *map = 0;
  UINTN map_key = 0;
  UINTN desc_size = 0;
  uint32_t desc_ver = 0;
  static uint8_t map_buffer[16384];
  map = (EFI_MEMORY_DESCRIPTOR *)map_buffer;

  SystemTable->ConOut->OutputString(
      SystemTable->ConOut, (uint16_t *)L"Exiting Boot Services loop...\r\n");

  while (1) {
    map_size = sizeof(map_buffer);
    status = SystemTable->BootServices->GetMemoryMap(&map_size, map, &map_key,
                                                     &desc_size, &desc_ver);
    if (status != EFI_SUCCESS) {
      SystemTable->ConOut->OutputString(
          SystemTable->ConOut, (uint16_t *)L"GetMemoryMap failed!\r\n");
      while (1)
        ;
    }

    status = SystemTable->BootServices->ExitBootServices(ImageHandle, map_key);
    if (status == EFI_SUCCESS) {
      break;
    }
  }

  KernelMain(gop->Mode->FrameBufferBase, gop->Mode->Info->HorizontalResolution,
             gop->Mode->Info->VerticalResolution,
             gop->Mode->Info->PixelsPerScanLine, map_size, map, map_key,
             desc_size, desc_ver, loaded_image->ImageBase,
             loaded_image->ImageSize, rsdp);

  return EFI_SUCCESS;
}
