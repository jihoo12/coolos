#include "timer.h"
#include "apic.h"
#include <stddef.h>

static uint64_t g_ticks = 0;

void Timer_Handler() {
  g_ticks++;
  LAPIC_SendEOI();
}

uint64_t Timer_GetTicks() { return g_ticks; }

void Timer_Sleep(uint64_t ms) {
  uint64_t target = g_ticks + ms;
  while (g_ticks < target) {
    asm volatile("hlt");
  }
}
