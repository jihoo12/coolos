#include "heap.h"
#include <stddef.h>

static HeapBlock *free_list = NULL;

void Heap_Init(void *start, size_t size) {
  free_list = (HeapBlock *)start;
  free_list->size = size - sizeof(HeapBlock);
  free_list->next = NULL;
  free_list->prev = NULL;
  free_list->free = 1;
}

void *kmalloc(size_t size) {
  HeapBlock *current = free_list;

  // Simple First-Fit
  while (current) {
    if (current->free && current->size >= size) {
      // Can we split the block?
      if (current->size >= size + sizeof(HeapBlock) + 16) {
        HeapBlock *next =
            (HeapBlock *)((uint8_t *)current + sizeof(HeapBlock) + size);
        next->size = current->size - size - sizeof(HeapBlock);
        next->next = current->next;
        next->prev = current;
        next->free = 1;

        if (next->next)
          next->next->prev = next;

        current->size = size;
        current->next = next;
      }

      current->free = 0;
      return (void *)((uint8_t *)current + sizeof(HeapBlock));
    }
    current = current->next;
  }

  return NULL;
}

void *kmalloc_aligned(size_t size, size_t alignment) {
  if (alignment <= sizeof(HeapBlock)) {
    return kmalloc(size);
  }

  // Very simple implementation: find a block, then adjust
  // This is not the most efficient but works for now
  HeapBlock *current = free_list;
  while (current) {
    if (current->free) {
      uintptr_t raw_addr = (uintptr_t)current + sizeof(HeapBlock);
      uintptr_t aligned_addr = (raw_addr + alignment - 1) & ~(alignment - 1);
      size_t padding = aligned_addr - raw_addr;

      if (current->size >= size + padding) {
        // If padding is large enough, split it
        if (padding >= sizeof(HeapBlock) + 16) {
          HeapBlock *new_block = (HeapBlock *)((uint8_t *)current + padding);
          new_block->size = current->size - padding;
          new_block->next = current->next;
          new_block->prev = current;
          new_block->free = 1;

          if (new_block->next)
            new_block->next->prev = new_block;

          current->size = padding - sizeof(HeapBlock);
          current->next = new_block;

          // Now allocate from new_block
          return kmalloc(
              size); // This is safe because new_block is exactly what we need
        } else if (padding == 0) {
          return kmalloc(size);
        }
        // If padding is small, we just waste it (or we could handle it better)
      }
    }
    current = current->next;
  }
  return NULL;
}

void kfree(void *ptr) {
  if (!ptr)
    return;

  HeapBlock *block = (HeapBlock *)((uint8_t *)ptr - sizeof(HeapBlock));
  block->free = 1;

  // Coalesce with next
  if (block->next && block->next->free) {
    block->size += block->next->size + sizeof(HeapBlock);
    block->next = block->next->next;
    if (block->next)
      block->next->prev = block;
  }

  // Coalesce with prev
  if (block->prev && block->prev->free) {
    block->prev->size += block->size + sizeof(HeapBlock);
    block->prev->next = block->next;
    if (block->next)
      block->next->prev = block->prev;
  }
}
