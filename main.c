#include "acpi.h"
#include "apic.h"
#include "efi.h"
#include "gdt.h"
#include "graphics.h"
#include "interrupt.h"
#include "libc.h"
#include "memory.h"
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

// Simple KernelMain function
void KernelMain(EFI_PHYSICAL_ADDRESS fb_base, uint32_t width, uint32_t height,
                uint32_t ppsl, UINTN map_size, EFI_MEMORY_DESCRIPTOR *map,
                UINTN map_key, UINTN desc_size, uint32_t desc_ver,
                void *image_base, uint64_t image_size, RSDP *rsdp) {

  PageAllocator_Init(map, map_size, desc_size);
  // Protect the kernel binary area
  PageAllocator_MarkUsed(image_base, (image_size + 4095) / 4096);

  // Initialize ACPI
  if (rsdp) {
    ACPI_Init(rsdp);
  }

  // Initialize GDT
  GDT_Init();

  // Initialize IDT
  IDT_Init();

  Graphics_Init(fb_base, width, height, ppsl);

  // Initialize 4-level Page Table with minimal mappings
  PageTable_Init(image_base, image_size, (void *)fb_base,
                 (uint64_t)ppsl * height * 4, map, map_size, desc_size);

  // Initialize Heap (e.g., 16MB)
  void *heap_addr = PageAllocator_Alloc(4096); // 4096 * 4KB = 16MB
  if (heap_addr) {
    Heap_Init(heap_addr, 4096 * 4096);
  }

  Graphics_Clear(0xEEE8D5); // Solarized Light
  Graphics_Print(100, 100, "HELLO FROM COOLOS KERNEL!", 0x268BD2); // Blue
  Graphics_Print(100, 150, "PAGES: ", 0x268BD2);
  Graphics_PrintHex(200, 150, total_pages, 0x268BD2);

  // Display Kernel binary location
  Graphics_Print(100, 175, "KERNEL BASE: ", 0x268BD2);
  Graphics_PrintHex(250, 175, (uintptr_t)image_base, 0x268BD2);

  // 2. 가용 메모리 계산 로직
  uint64_t free_pages = 0;
  for (uint64_t i = 0; i < total_pages; i++) {
    if (!(bitmap[i / 8] & (1 << (i % 8)))) {
      free_pages++;
    }
  }

  // 3. MB 단위 출력 (계산 결과를 변수에 담아 출력)
  uint64_t mb = (free_pages * 4096) / 1024 / 1024;
  Graphics_Print(100, 225, "FREE MB: ", 0x268BD2);
  Graphics_PrintHex(250, 225, mb, 0x268BD2);

  // Heap Test
  void *ptr1 = kmalloc(100);
  void *ptr2 = kmalloc(200);
  void *ptr3 = kmalloc_aligned(1024, 4096); // Page aligned
  if (ptr1 && ptr2 && ptr3) {
    Graphics_Print(100, 275, "HEAP ALLOC SUCCESS", 0x268BD2);
    if (((uintptr_t)ptr3 % 4096) == 0) {
      Graphics_Print(100, 300, "ALIGNED ALLOC SUCCESS", 0x268BD2);
    }
    kfree(ptr1);
    kfree(ptr2);
    kfree(ptr3);
    Graphics_Print(100, 325, "HEAP FREE SUCCESS", 0x268BD2);
  }

  if (rsdp) {
    Graphics_Print(100, 350, "RSDP FOUND AT: ", 0x268BD2);
    Graphics_PrintHex(300, 350, (uintptr_t)rsdp, 0x268BD2);

    FADT *fadt = (FADT *)ACPI_FindTable("FACP"); // FADT signature is "FACP"
    if (fadt) {
      Graphics_Print(100, 375, "FADT FOUND", 0x268BD2);
    }

    MADT *madt = (MADT *)ACPI_FindTable("APIC"); // MADT signature is "APIC"
    if (madt) {
      Graphics_Print(100, 400, "MADT FOUND", 0x268BD2);
      Graphics_Print(100, 425, "LAPIC ADDR: ", 0x268BD2);
      Graphics_PrintHex(250, 425, madt->LocalApicAddress, 0x268BD2);

      // Initialize LAPIC
      LAPIC_Init((void *)(uintptr_t)madt->LocalApicAddress);

      // Calibrate Timer (get ticks per 10ms)
      uint32_t ticks_10ms = LAPIC_CalibrateTimer();
      // Initialize Timer for 1ms intervals
      LAPIC_TimerInit(ticks_10ms / 10);

      // Enable Interrupts
      asm volatile("sti");
      Graphics_Print(100, 450, "INTERRUPTS ENABLED", 0x268BD2);
      Graphics_Print(100, 475, "TICKS/1MS: ", 0x268BD2);
      Graphics_PrintHex(250, 475, ticks_10ms / 10, 0x268BD2);
    }
  } else {
    Graphics_Print(100, 350, "RSDP NOT FOUND", 0xDC322F); // Red
  }

  while (1) {
    Graphics_Print(100, 500, "TICKS: ", 0x268BD2);
    Graphics_PrintHex(200, 500, Timer_GetTicks(), 0x268BD2);
    for (int i = 0; i < 1000000; i++)
      asm("pause"); // Simple delay for visibility
  }
}

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
