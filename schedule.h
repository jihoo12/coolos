#ifndef SCHEDULE_H
#define SCHEDULE_H

#include "interrupt.h"
#include <stdint.h>

#define MAX_TASKS 10

typedef struct {
  uint64_t rsp;
  int active;
} TCB;

void Scheduler_Init();
void Scheduler_AddTask(void (*fn)(), void *stack_base);
void Scheduler_Switch(InterruptFrame **frame_ptr);
void Scheduler_TerminateCurrentTask(InterruptFrame **frame_ptr);
#endif
