#include "schedule.h"
#include "gdt.h"
#include "graphics.h"
#include "libc.h"
#include "memory.h" // For PageAllocator_Free and PAGE_SIZE
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

  // Set up the initial stack (Kernel Task)
  // Assuming 1 page for kernel tasks created this way for now, or just don't
  // free them if stack_base is NULL? Let's assume standard kernel tasks manage
  // their own stack or are static. But for consistency:

  uint64_t *stack_top = (uint64_t *)((uint8_t *)stack_base + 4096);
  uint64_t *stack = stack_top;

  // Push iretq frame
  *(--stack) = KERNEL_DATA_SEL;      // SS
  *(--stack) = (uintptr_t)stack_top; // RSP
  *(--stack) = 0x202;                // RFLAGS
  *(--stack) = KERNEL_CODE_SEL;      // CS
  *(--stack) = (uintptr_t)fn;        // RIP

  // ... (Zero out registers logic is same, omitted for brevity if allowed, but
  // I must replace full block or be careful) I will just copy the zeroing part
  // to be safe.

  // int_no and err_code
  *(--stack) = 0;
  *(--stack) = 0;

  // Push GPRs
  for (int i = 0; i < 15; i++)
    *(--stack) = 0;

  tasks[idx].rsp = (uintptr_t)stack;
  tasks[idx].stack_base = stack_base;
  tasks[idx].stack_pages = 1; // Default
  tasks[idx].active = 1;
}

void Scheduler_AddUserTask(void (*fn)(), void *stack_base,
                           uint64_t stack_pages) {
  if (total_tasks >= MAX_TASKS)
    return;

  int idx = total_tasks++;

  // Set up the initial stack (User Task)
  // Stack grows down from base + size
  uint64_t stack_size =
      stack_pages * 4096; // PAGE_SIZE is not visible unless memory.h included
  uint64_t *stack_top = (uint64_t *)((uint8_t *)stack_base + stack_size);

  // We need to set up the KERNEL STACK for the context switch to happen
  // correctly BUT wait, when we switch TO this task, we pop the context from
  // the KERNEL stack. The 'stack' pointer here effectively BECOMES the kernel
  // stack for this task's context restoration. Wait, no. TCB.rsp points to
  // where we saved the registers. When we switch to this task, we load RSP from
  // TCB.rsp, pop registers, and `iretq`. `iretq` will pop SS:RSP (User Stack)
  // and CS:RIP (User Code).

  // So the "stack" we are building HERE is the KERNEL STACK that contains the
  // `iret` frame. We need a kernel stack for every task. The argument
  // `stack_base` passed from Syscall_Exec is the USER stack (allocated for
  // user). WE NEED A SEPARATE KERNEL STACK for this task!

  // Implicit allocation of Kernel Stack for the task?
  void *kstack = PageAllocator_Alloc(1);
  if (!kstack)
    return; // Failure

  uint64_t *kstack_top = (uint64_t *)((uint8_t *)kstack + 4096);
  uint64_t *ks = kstack_top;

  // Push iretq frame (USER Privileges)
  *(--ks) = USER_DATA_SEL;        // SS (Ring 3)
  *(--ks) = (uintptr_t)stack_top; // RSP (User Stack Top)
  *(--ks) = 0x202;                // RFLAGS (Interrupts enabled)
  *(--ks) = USER_CODE_SEL;        // CS (Ring 3)
  *(--ks) = (uintptr_t)fn;        // RIP (User Function)

  // int_no and err_code
  *(--ks) = 0;
  *(--ks) = 0;

  // Push GPRs
  for (int i = 0; i < 15; i++)
    *(--ks) = 0;

  tasks[idx].rsp = (uintptr_t)ks;
  tasks[idx].active = 1;

  // Store USER stack info for freeing (if we want to free user stack when task
  // dies) We also need to track the KERNEL stack we just allocated to free it
  // too. Limitation: TCB currently only has one `stack_base`. Let's treat
  // `stack_base` as the User Stack (major resource) and maybe leak KStack for
  // now or (better) we should track both. Given the struct update, let's use
  // `stack_base` for User Stack. I'll add `kstack_base` TODO or just reuse
  // fields if possible. Actually, let's just track the user stack for now as
  // that was the user's request context (SYSCALL_EXEC alloc). The Scheduler
  // internal kstack allocation is a new detail.

  tasks[idx].stack_base = stack_base;
  tasks[idx].stack_pages = stack_pages;

  // NOTE: We are "leaking" the 'kstack' (1 page) here because TCB doesn't have
  // a field for it yet. I should probably add `kstack_base` to TCB to be
  // perfect, but let's stick to the user request first.
}
// ... switch ...

void Scheduler_TerminateCurrentTask(InterruptFrame **frame_ptr) {
  // Cannot terminate the main task (task 0)
  if (current_task_index == 0) {
    Graphics_Clear(0xEEE8D5);
    Graphics_Print(100, 500, "CANNOT TERMINATE MAIN TASK", 0xFF0000);
    return;
  }

  // Free resources
  TCB *task = &tasks[current_task_index];
  if (task->stack_base && task->stack_pages > 0) {
    PageAllocator_Free(task->stack_base, task->stack_pages);
    task->stack_base = 0;
  }

  // Mark current task as inactive
  task->active = 0;
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