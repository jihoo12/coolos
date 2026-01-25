#include "ioapic.h"

static void *g_ioapic_base = 0;

static void ioapic_write(uint8_t reg, uint32_t val) {
  volatile uint32_t *iowin = (uint32_t *)((uint8_t *)g_ioapic_base + 0x10);
  volatile uint32_t *ioregsel = (uint32_t *)((uint8_t *)g_ioapic_base + 0x00);
  *ioregsel = reg;
  *iowin = val;
}

static uint32_t ioapic_read(uint8_t reg) {
  volatile uint32_t *iowin = (uint32_t *)((uint8_t *)g_ioapic_base + 0x10);
  volatile uint32_t *ioregsel = (uint32_t *)((uint8_t *)g_ioapic_base + 0x00);
  *ioregsel = reg;
  return *iowin;
}

void IOAPIC_Init(void *base) { g_ioapic_base = base; }

void IOAPIC_MapIRQ(uint8_t irq, uint8_t vector, uint8_t apic_id) {
  uint32_t low =
      vector; // Delivery Mode: Fixed (0), Destination Mode: Physical (0)
  uint32_t high = (uint32_t)apic_id << 24;

  ioapic_write(IOAPIC_REG_REDTBL + 2 * irq, low);
  ioapic_write(IOAPIC_REG_REDTBL + 2 * irq + 1, high);
}
