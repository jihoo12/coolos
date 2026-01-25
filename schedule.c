#include "schedule.h"
#include "gdt.h"
#include "graphics.h"
#include "libc.h"

static TCB tasks[MAX_TASKS];
static int current_task_index = 0;
static int total_tasks = 0;

void Scheduler_Init() {
  for (int i = 0; i < MAX_TASKS; i++) {
    tasks[i].active = 0;
  }
  tasks[0].active = 1; // Main thread
  total_tasks = 1;
}

void Scheduler_AddTask(void (*fn)(), void *stack_base) {
  if (total_tasks >= MAX_TASKS)
    return;

  int idx = total_tasks++;

  // Set up the initial stack
  uint64_t *stack_top = (uint64_t *)((uint8_t *)stack_base + 4096);
  uint64_t *stack = stack_top;

  // Push iretq frame
  *(--stack) = KERNEL_DATA_SEL;      // SS
  *(--stack) = (uintptr_t)stack_top; // RSP (Original top)
  *(--stack) = 0x202;                // RFLAGS (Interrupt flag set)
  *(--stack) = KERNEL_CODE_SEL;      // CS
  *(--stack) = (uintptr_t)fn;        // RIP

  // int_no and err_code
  *(--stack) = 0; // err_code
  *(--stack) = 0; // int_no

  // Push GPRs (InterruptFrame)
  *(--stack) = 0; // rax
  *(--stack) = 0; // rbx
  *(--stack) = 0; // rcx
  *(--stack) = 0; // rdx
  *(--stack) = 0; // rbp
  *(--stack) = 0; // rdi
  *(--stack) = 0; // rsi
  *(--stack) = 0; // r8
  *(--stack) = 0; // r9
  *(--stack) = 0; // r10
  *(--stack) = 0; // r11
  *(--stack) = 0; // r12
  *(--stack) = 0; // r13
  *(--stack) = 0; // r14
  *(--stack) = 0; // r15

  tasks[idx].rsp = (uintptr_t)stack;
  tasks[idx].active = 1;
}

void Scheduler_Switch(InterruptFrame **frame_ptr) {
  int next_index = -1;
  // Search for next active task (check all tasks including wrap-around)
  for (int i = 1; i <= total_tasks; i++) {
    int idx = (current_task_index + i) % total_tasks;
    if (tasks[idx].active && idx != current_task_index) {
      next_index = idx;
      break;
    }
  }

  if (next_index == -1 || next_index == current_task_index) {
    Graphics_Clear(0xEEE8D5);
    Graphics_Print(100, 500, "NO NEXT TASK AVAILABLE", 0xFF0000);
    return;
  }

  // Save current task RSP
  tasks[current_task_index].rsp = (uintptr_t)*frame_ptr;

  // Move to next active task
  current_task_index = next_index;

  // Restore next task RSP
  *frame_ptr = (InterruptFrame *)tasks[current_task_index].rsp;
  Graphics_Clear(0xEEE8D5);
  Graphics_Print(100, 500, "SWITCHED TO NEXT TASK     ", 0x00FF00);
}

void Scheduler_TerminateCurrentTask(InterruptFrame **frame_ptr) {
  // Cannot terminate the main task (task 0)
  if (current_task_index == 0) {
    Graphics_Clear(0xEEE8D5);
    Graphics_Print(100, 500, "CANNOT TERMINATE MAIN TASK", 0xFF0000);
    return;
  }

  // Mark current task as inactive
  tasks[current_task_index].active = 0;
  Graphics_Clear(0xEEE8D5);
  Graphics_Print(100, 500, "TASK TERMINATED            ", 0xFFFF00);

  // Find next active task
  int next_index = -1;
  for (int i = 1; i <= total_tasks; i++) {
    int idx = (current_task_index + i) % total_tasks;
    if (tasks[idx].active) {
      next_index = idx;
      break;
    }
  }

  // If no next task, go back to main task (task 0)
  if (next_index == -1) {
    next_index = 0;
  }

  // Move to next task
  current_task_index = next_index;

  // Restore next task RSP
  *frame_ptr = (InterruptFrame *)tasks[current_task_index].rsp;
}
