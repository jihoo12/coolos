#include "apic.h"
#include "io.h"
#include <stddef.h>

#define PIT_FREQ 1193182

static void *g_lapic_base = NULL;

static void lapic_write(uint32_t reg, uint32_t val) {
  *(volatile uint32_t *)((uint8_t *)g_lapic_base + reg) = val;
}

static uint32_t lapic_read(uint32_t reg) {
  return *(volatile uint32_t *)((uint8_t *)g_lapic_base + reg);
}

void LAPIC_Init(void *base) {
  g_lapic_base = base;

  // 1. Disable legacy PIC
  outb(0xA1, 0xFF);
  outb(0x21, 0xFF);

  // 2. Set Spurious Interrupt Vector Register
  // Enable APIC by setting bit 8 and set spurious vector to 0xFF
  lapic_write(LAPIC_REG_SIVR, lapic_read(LAPIC_REG_SIVR) | 0x1FF);
}

// Reliable PIT delay using Channel 2 and the speaker port
static void pit_delay(uint16_t ms) {
  uint32_t count = (PIT_FREQ / 1000) * ms;
  if (count > 0xFFFF)
    count = 0xFFFF;
  outb(0x43, 0xB2); // Ch 2, lobyte/hibyte, mode 0
  outb(0x42, count & 0xFF);
  outb(0x42, (count >> 8) & 0xFF);

  uint8_t val = inb(0x61);
  outb(0x61, val & 0xFD); // Disable speaker
  outb(0x61, val | 1);    // Enable PIT2 gate

  while (!(inb(0x61) & 0x20))
    ; // Wait for PIT2 output to go high

  outb(0x61, val & 0xFC); // Stop PIT2
}

uint32_t LAPIC_CalibrateTimer() {
  lapic_write(LAPIC_REG_TDCR, 0x3);        // Div 16
  lapic_write(LAPIC_REG_TICR, 0xFFFFFFFF); // Start value

  pit_delay(10); // Wait 10ms

  uint32_t current = lapic_read(LAPIC_REG_TCCR);
  uint32_t ticks = 0xFFFFFFFF - current;

  lapic_write(LAPIC_REG_TICR, 0); // Stop timer
  return ticks;                   // Ticks per 10ms
}

void LAPIC_TimerInit(uint32_t count) {
  // 1. Set Divider to 16
  lapic_write(LAPIC_REG_TDCR, 0x3);

  // 2. Set LVT Timer to Periodic and Vector 0x40
  lapic_write(LAPIC_REG_LVT_TIMER, 0x40 | LAPIC_TIMER_PERIODIC);

  // 3. Set Initial Count
  lapic_write(LAPIC_REG_TICR, count);
}

void LAPIC_SendEOI(void) { lapic_write(LAPIC_REG_EOI, 0); }
