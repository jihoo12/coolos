#include "gdt.h"

static GDTEntry gdt[3];
static GDTPointer gdt_ptr;

void GDT_SetEntry(int index, uint32_t base, uint32_t limit, uint8_t access,
                  uint8_t gran) {
  gdt[index].base_low = (base & 0xFFFF);
  gdt[index].base_middle = (base >> 16) & 0xFF;
  gdt[index].base_high = (base >> 24) & 0xFF;

  gdt[index].limit_low = (limit & 0xFFFF);
  gdt[index].granularity = (limit >> 16) & 0x0F;

  gdt[index].granularity |= gran & 0xF0;
  gdt[index].access = access;
}

void GDT_Init() {
  // Null descriptor
  GDT_SetEntry(0, 0, 0, 0, 0);

  // Kernel Code Segment: Access 0x9A, Granularity 0xAF (64-bit)
  // Access: Present(1), DPL(00), Type(1), Code(1), Conforming(0), Readable(1),
  // Accessed(0) -> 1001 1010 = 0x9A Granularity: G(1), D(0), L(1), AVL(0) ->
  // 1010 -> 0xA (+ limit high 0xF) -> 0xAF
  GDT_SetEntry(1, 0, 0xFFFFFFFF, 0x9A, 0xAF);

  // Kernel Data Segment: Access 0x92, Granularity 0xCF
  // Access: Present(1), DPL(00), Type(1), Data(1), Expansion-direction(0),
  // Writable(1), Accessed(0) -> 1001 0010 = 0x92 Granularity: G(1), D(1), L(0),
  // AVL(0) -> 1100 -> 0xC
  GDT_SetEntry(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

  gdt_ptr.limit = sizeof(gdt) - 1;
  gdt_ptr.base = (uint64_t)&gdt;

  // Load GDT and Update Segments
  asm volatile("lgdt %0\n\t"
               "pushq %1\n\t"
               "leaq 1f(%%rip), %%rax\n\t"
               "pushq %%rax\n\t"
               "lretq\n\t"
               "1:\n\t"
               "mov %2, %%ds\n\t"
               "mov %2, %%es\n\t"
               "mov %2, %%fs\n\t"
               "mov %2, %%gs\n\t"
               "mov %2, %%ss\n\t"
               :
               : "m"(gdt_ptr), "i"(KERNEL_CODE_SEL), "r"(KERNEL_DATA_SEL)
               : "rax", "memory");
}
