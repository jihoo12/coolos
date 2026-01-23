#ifndef GDT_H
#define GDT_H

#include <stdint.h>

typedef struct {
  uint16_t limit_low;
  uint16_t base_low;
  uint8_t base_middle;
  uint8_t access;
  uint8_t granularity;
  uint8_t base_high;
} __attribute__((packed)) GDTEntry;

typedef struct {
  uint16_t limit;
  uint64_t base;
} __attribute__((packed)) GDTPointer;

#define KERNEL_CODE_SEL 0x08
#define KERNEL_DATA_SEL 0x10

void GDT_Init();

#endif
