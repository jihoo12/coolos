#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void Timer_Handler();
void Timer_Sleep(uint64_t ms);
uint64_t Timer_GetTicks();

#endif
