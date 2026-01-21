#include "memory.h"
#include <stddef.h>

uint8_t bitmap[MAX_PAGES / 8];
uint64_t total_pages = 0;

void PageAllocator_Init(EFI_MEMORY_DESCRIPTOR *map, UINTN map_size,
                        UINTN desc_size) {
  // Clear bitmap (mark all as used initially)
  for (int i = 0; i < sizeof(bitmap); i++)
    bitmap[i] = 0xFF;

  EFI_MEMORY_DESCRIPTOR *entry = map;
  void *map_end = (void *)((uint8_t *)map + map_size);

  while ((void *)entry < map_end) {
    if (entry->Type == EfiConventionalMemory) {
      uint64_t start_page = entry->PhysicalStart / PAGE_SIZE;
      uint64_t num_pages = entry->NumberOfPages;

      for (uint64_t i = 0; i < num_pages; i++) {
        uint64_t page = start_page + i;
        if (page < MAX_PAGES) {
          bitmap[page / 8] &= ~(1 << (page % 8));
          if (page >= total_pages)
            total_pages = page + 1;
        }
      }
    }
    entry = (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)entry + desc_size);
  }
}

void PageAllocator_MarkUsed(void *ptr, UINTN pages) {
  uint64_t start_page = (uintptr_t)ptr / PAGE_SIZE;
  for (uint64_t i = 0; i < pages; i++) {
    uint64_t page = start_page + i;
    if (page < MAX_PAGES) {
      bitmap[page / 8] |= (1 << (page % 8));
    }
  }
}

void *PageAllocator_Alloc(UINTN pages) {
  if (pages == 0)
    return NULL;

  for (uint64_t i = 0; i <= total_pages - pages; i++) {
    uint64_t found = 0;
    for (uint64_t j = 0; j < pages; j++) {
      if (bitmap[(i + j) / 8] & (1 << ((i + j) % 8))) {
        break;
      }
      found++;
    }

    if (found == pages) {
      // Mark as used
      for (uint64_t j = 0; j < pages; j++) {
        bitmap[(i + j) / 8] |= (1 << ((i + j) % 8));
      }
      return (void *)(i * PAGE_SIZE);
    }
  }

  return NULL;
}

void PageAllocator_Free(void *ptr, UINTN pages) {
  uint64_t start_page = (uintptr_t)ptr / PAGE_SIZE;
  for (uint64_t i = 0; i < pages; i++) {
    uint64_t page = start_page + i;
    if (page < MAX_PAGES) {
      bitmap[page / 8] &= ~(1 << (page % 8));
    }
  }
}
