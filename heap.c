#include "heap.h"
#include <stddef.h>

static HeapBlock *free_list = NULL;

void Heap_Init(void *start, size_t size) {
  free_list = (HeapBlock *)start;
  free_list->size = size - sizeof(HeapBlock);
  free_list->next = NULL;
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
        next->free = 1;

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

void kfree(void *ptr) {
  if (!ptr)
    return;

  HeapBlock *block = (HeapBlock *)((uint8_t *)ptr - sizeof(HeapBlock));
  block->free = 1;

  // Coalesce adjacent free blocks
  HeapBlock *current = free_list;
  while (current) {
    if (current->free && current->next && current->next->free) {
      current->size += current->next->size + sizeof(HeapBlock);
      current->next = current->next->next;
      // Don't advance current, try to coalesce again with the new next
      continue;
    }
    current = current->next;
  }
}
