#include "efi.h"
#include "gdt.h"
#include "graphics.h"
#include "memory.h"

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
                void *image_base, uint64_t image_size) {

  PageAllocator_Init(map, map_size, desc_size);
  // Protect the kernel binary area
  PageAllocator_MarkUsed(image_base, (image_size + 4095) / 4096);

  // Initialize GDT
  GDT_Init();

  Graphics_Init(fb_base, width, height, ppsl);

  // Initialize 4-level Page Table with minimal mappings
  PageTable_Init(image_base, image_size, (void *)fb_base,
                 (uint64_t)ppsl * height * 4);

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
  while (1)
    ;
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
             loaded_image->ImageSize);

  return EFI_SUCCESS;
}
