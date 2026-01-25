#ifndef IOAPIC_H
#define IOAPIC_H

#include <stdint.h>

#define IOAPIC_REG_ID 0x00
#define IOAPIC_REG_VER 0x01
#define IOAPIC_REG_ARB 0x02
#define IOAPIC_REG_REDTBL 0x10

void IOAPIC_Init(void *base);
void IOAPIC_MapIRQ(uint8_t irq, uint8_t vector, uint8_t apic_id);

#endif
