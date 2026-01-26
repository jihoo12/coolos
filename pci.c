#include "pci.h"
#include "graphics.h"
#include "io.h"
#include "libc.h"
#include "memory.h"

static PCI_Device g_nvme_device = {0};
static int g_nvme_found = 0;

// Internal helper for ports (assuming they are defined in io.h or inline
// assembly needed)
static inline void outl(uint16_t port, uint32_t val) {
  asm volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
  uint32_t ret;
  asm volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

uint32_t PCI_Read32(uint8_t bus, uint8_t device, uint8_t function,
                    uint8_t offset) {
  uint32_t address = (uint32_t)((bus << 16) | (device << 11) | (function << 8) |
                                (offset & 0xFC) | ((uint32_t)0x80000000));
  outl(PCI_CONFIG_ADDRESS, address);
  return inl(PCI_CONFIG_DATA);
}

uint16_t PCI_Read16(uint8_t bus, uint8_t device, uint8_t function,
                    uint8_t offset) {
  uint32_t val = PCI_Read32(bus, device, function, offset);
  return (uint16_t)((val >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t PCI_Read8(uint8_t bus, uint8_t device, uint8_t function,
                  uint8_t offset) {
  uint32_t val = PCI_Read32(bus, device, function, offset);
  return (uint8_t)((val >> ((offset & 3) * 8)) & 0xFF);
}

void PCI_Write32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset,
                 uint32_t value) {
  uint32_t address = (uint32_t)((bus << 16) | (device << 11) | (function << 8) |
                                (offset & 0xFC) | ((uint32_t)0x80000000));
  outl(PCI_CONFIG_ADDRESS, address);
  outl(PCI_CONFIG_DATA, value);
}

// Check a specific function of a device
void PCI_CheckFunction(uint8_t bus, uint8_t device, uint8_t function) {
  uint8_t classCode = PCI_Read8(bus, device, function, 0x0B);
  uint8_t subClass = PCI_Read8(bus, device, function, 0x0A);
  uint8_t progIF = PCI_Read8(bus, device, function, 0x09);

  if (classCode == PCI_CLASS_STORAGE && subClass == PCI_SUBCLASS_NVME &&
      progIF == PCI_PROG_IF_NVME) {
    g_nvme_device.Bus = bus;
    g_nvme_device.Device = device;
    g_nvme_device.Function = function;
    g_nvme_device.VendorID = PCI_Read16(bus, device, function, 0x00);
    g_nvme_device.DeviceID = PCI_Read16(bus, device, function, 0x02);

    // Read BAR0 (Memory Mapped IO Base Address)
    // BAR0 is at offset 0x10, BAR1 at 0x14. NVMe 64-bit BAR is usually BAR0/1.
    uint32_t bar0 = PCI_Read32(bus, device, function, 0x10);
    uint32_t bar1 = PCI_Read32(bus, device, function, 0x14);

    // Mask out the Type/Prefetchable bits (low 4 bits usually) to get address
    // For 64-bit BAR, it's (bar1 << 32) | (bar0 & 0xFFFFFFF0)
    // But for simplicity in this 32/64 bit hybrid env, we just store raw or
    // easy parse.
    g_nvme_device.BaseAddress[0] = bar0;
    g_nvme_device.BaseAddress[1] = bar1;

    g_nvme_found = 1;

    // Enable Bus Master and Memory Space in Command Register (Offset 0x04)
    uint16_t cmd = PCI_Read16(bus, device, function, 0x04);
    cmd |= 0x0006; // Bit 1: Memory Space, Bit 2: Bus Master
    PCI_Write32(bus, device, function, 0x04,
                cmd); // Write back 32 bits (safe for command/status)

    // Graphics_Print(100, 600, "PCI: FOUND NVME CONTROLLER!", 0x00FF00);
  }
}

void PCI_CheckDevice(uint8_t bus, uint8_t device) {
  uint16_t vendorID = PCI_Read16(bus, device, 0, 0x00);
  if (vendorID == 0xFFFF)
    return; // Device doesn't exist

  PCI_CheckFunction(bus, device, 0);

  uint8_t headerType = PCI_Read8(bus, device, 0, 0x0E);
  if ((headerType & 0x80) != 0) {
    // Multi-function device
    for (uint8_t func = 1; func < 8; func++) {
      if (PCI_Read16(bus, device, func, 0x00) != 0xFFFF) {
        PCI_CheckFunction(bus, device, func);
      }
    }
  }
}

void PCI_ScanBus() {
  for (uint16_t bus = 0; bus < 256; bus++) {
    for (uint8_t dev = 0; dev < 32; dev++) {
      PCI_CheckDevice(bus, dev);
    }
  }
}

void PCI_Init() { PCI_ScanBus(); }

PCI_Device *PCI_GetNVMeController() {
  if (g_nvme_found)
    return &g_nvme_device;
  return NULL;
}

uint32_t PCI_GetBAR(PCI_Device *dev, int barNum) {
  // For NVMe, BAR0/1 form the 64-bit address. Return low 32 for now or handle
  // appropriately. In this basic OS, assuming mapped strictly in 4GB or
  // handling 64bit logic elsewhere.
  return dev->BaseAddress[barNum];
}
