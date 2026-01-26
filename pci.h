#ifndef PCI_H
#define PCI_H

#include <stdint.h>

// PCI Configuration Address Port
#define PCI_CONFIG_ADDRESS 0xCF8
// PCI Configuration Data Port
#define PCI_CONFIG_DATA    0xCFC

// PCI Header Types
#define PCI_HEADER_TYPE_NORMAL 0x00
#define PCI_HEADER_TYPE_BRIDGE 0x01
#define PCI_HEADER_TYPE_CARDBUS 0x02

// PCI Class Codes
#define PCI_CLASS_STORAGE 0x01

// PCI Subclass Codes
#define PCI_SUBCLASS_NVME 0x08

// PCI Programming Interface
#define PCI_PROG_IF_NVME 0x02

typedef struct {
    uint16_t VendorID;
    uint16_t DeviceID;
    uint16_t Command;
    uint16_t Status;
    uint8_t  RevisionID;
    uint8_t  ProgIF;
    uint8_t  Subclass;
    uint8_t  ClassCode;
    uint8_t  CacheLineSize;
    uint8_t  LatencyTimer;
    uint8_t  HeaderType;
    uint8_t  BIST;
} __attribute__((packed)) PCI_CommonHeader;

typedef struct {
    uint8_t Bus;
    uint8_t Device;
    uint8_t Function;
    uint16_t VendorID;
    uint16_t DeviceID;
    uint32_t BaseAddress[6]; // BAR0-BAR5
} PCI_Device;

// Function Prototypes
void PCI_Init();
void PCI_ScanBus();
PCI_Device* PCI_GetNVMeController();
uint32_t PCI_Read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint16_t PCI_Read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint8_t  PCI_Read8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void PCI_Write32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
uint32_t PCI_GetBAR(PCI_Device* dev, int barNum);

#endif
