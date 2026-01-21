#ifndef MEMORY_H
#define MEMORY_H

#include "efi.h"
#define PAGE_SIZE 4096
#define MAX_PAGES                                                              \
  (1024 * 1024) // Support up to 4GB for now (1024*1024 * 4KB = 4GB)
extern uint8_t bitmap[MAX_PAGES / 8];
extern uint64_t total_pages;

void PageAllocator_Init(EFI_MEMORY_DESCRIPTOR *map, UINTN map_size,
                        UINTN desc_size);
void PageAllocator_MarkUsed(void *ptr, UINTN pages);
void *PageAllocator_Alloc(UINTN pages);
void PageAllocator_Free(void *ptr, UINTN pages);

#endif
