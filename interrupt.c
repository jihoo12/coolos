#include "interrupt.h"
#include "gdt.h"
#include "graphics.h"

static IDTEntry idt[256];
static IDTPointer idt_ptr;

extern void isr0();
extern void isr6();
extern void isr8();
extern void isr13();
extern void isr14();

void IDT_SetGate(uint8_t vector, void *handler, uint16_t selector,
                 uint8_t type_attr) {
  uintptr_t addr = (uintptr_t)handler;
  idt[vector].offset_low = addr & 0xFFFF;
  idt[vector].selector = selector;
  idt[vector].ist = 0;
  idt[vector].type_attr = type_attr;
  idt[vector].offset_mid = (addr >> 16) & 0xFFFF;
  idt[vector].offset_high = (addr >> 32) & 0xFFFFFFFF;
  idt[vector].zero = 0;
}

void ExceptionHandler(InterruptFrame *frame) {
  Graphics_Clear(0x3B5998); // Blue screenish
  Graphics_Print(100, 100, "EXCEPTION OCCURRED!", 0xFFFFFF);
  Graphics_Print(100, 130, "Interrupt No: ", 0xFFFFFF);
  Graphics_PrintHex(250, 130, frame->int_no, 0xFFFFFF);
  Graphics_Print(100, 160, "Error Code: ", 0xFFFFFF);
  Graphics_PrintHex(250, 160, frame->err_code, 0xFFFFFF);
  Graphics_Print(100, 190, "RIP: ", 0xFFFFFF);
  Graphics_PrintHex(250, 190, frame->rip, 0xFFFFFF);

  while (1)
    asm("hlt");
}

// Assembly stubs
asm(".global isr0\n"
    "isr0:\n"
    "  pushq $0\n" // dummy error code
    "  pushq $0\n" // int no
    "  jmp isr_common\n");

asm(".global isr6\n"
    "isr6:\n"
    "  pushq $0\n" // dummy error code
    "  pushq $6\n" // int no
    "  jmp isr_common\n");

asm(".global isr8\n"
    "isr8:\n"
    "  pushq $8\n" // int no (error code already pushed by CPU)
    "  jmp isr_common\n");

asm(".global isr13\n"
    "isr13:\n"
    "  pushq $13\n" // int no (error code already pushed by CPU)
    "  jmp isr_common\n");

asm(".global isr14\n"
    "isr14:\n"
    "  pushq $14\n" // int no (error code already pushed by CPU)
    "  jmp isr_common\n");

asm("isr_common:\n"
    "  pushq %rax\n"
    "  pushq %rbx\n"
    "  pushq %rcx\n"
    "  pushq %rdx\n"
    "  pushq %rbp\n"
    "  pushq %rdi\n"
    "  pushq %rsi\n"
    "  pushq %r8\n"
    "  pushq %r9\n"
    "  pushq %r10\n"
    "  pushq %r11\n"
    "  pushq %r12\n"
    "  pushq %r13\n"
    "  pushq %r14\n"
    "  pushq %r15\n"
    "  movq %rsp, %rcx\n" // First argument for Windows x64 ABI is RCX
    "  subq $32, %rsp\n"  // Shadow space for Windows ABI
    "  call ExceptionHandler\n"
    "  addq $32, %rsp\n"
    "  popq %r15\n"
    "  popq %r14\n"
    "  popq %r13\n"
    "  popq %r12\n"
    "  popq %r11\n"
    "  popq %r10\n"
    "  popq %r9\n"
    "  popq %r8\n"
    "  popq %rsi\n"
    "  popq %rdi\n"
    "  popq %rbp\n"
    "  popq %rdx\n"
    "  popq %rcx\n"
    "  popq %rbx\n"
    "  popq %rax\n"
    "  addq $16, %rsp\n" // Clean up int_no and err_code
    "  iretq\n");

void IDT_Init() {
  for (int i = 0; i < 256; i++) {
    // Zero out
    uint64_t *ptr = (uint64_t *)&idt[i];
    ptr[0] = 0;
    ptr[1] = 0;
  }

  // Type 0x8E = 1000 1110 (Present, DPL 0, Interrupt Gate)
  IDT_SetGate(0, isr0, KERNEL_CODE_SEL, 0x8E);
  IDT_SetGate(6, isr6, KERNEL_CODE_SEL, 0x8E);
  IDT_SetGate(8, isr8, KERNEL_CODE_SEL, 0x8E);
  IDT_SetGate(13, isr13, KERNEL_CODE_SEL, 0x8E);
  IDT_SetGate(14, isr14, KERNEL_CODE_SEL, 0x8E);

  idt_ptr.limit = sizeof(idt) - 1;
  idt_ptr.base = (uint64_t)&idt;

  asm volatile("lidt %0" : : "m"(idt_ptr));
}
