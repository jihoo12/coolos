#ifndef ACPI_H
#define ACPI_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
  char Signature[8];
  uint8_t Checksum;
  char OEMID[6];
  uint8_t Revision;
  uint32_t RsdtAddress;
  uint32_t Length;
  uint64_t XsdtAddress;
  uint8_t ExtendedChecksum;
  uint8_t Reserved[3];
} __attribute__((packed)) RSDP;

typedef struct {
  char Signature[4];
  uint32_t Length;
  uint8_t Revision;
  uint8_t Checksum;
  char OEMID[6];
  char OEMTableID[8];
  uint32_t OEMRevision;
  uint32_t CreatorID;
  uint32_t CreatorRevision;
} __attribute__((packed)) SDTHeader;

typedef struct {
  SDTHeader Header;
  uint64_t Tables[];
} __attribute__((packed)) XSDT;

typedef struct {
  char Signature[4];
  uint32_t Length;
  uint8_t Revision;
  uint8_t Checksum;
  char OEMID[6];
  char OEMTableID[8];
  uint32_t OEMRevision;
  uint32_t CreatorID;
  uint32_t CreatorRevision;
  uint32_t Address; // Physical address of the table
} __attribute__((packed)) GenericAddressStructure;

typedef struct {
  SDTHeader Header;
  uint32_t FirmwareCtrl;
  uint32_t Dsdt;
  uint8_t Reserved;
  uint8_t PreferredPMProfile;
  uint16_t SCI_Interrupt;
  uint32_t SMI_CommandPort;
  uint8_t AcpiEnable;
  uint8_t AcpiDisable;
  uint8_t S4BiosReq;
  uint8_t PstateControl;
  uint32_t Pm1aEventBlock;
  uint32_t Pm1bEventBlock;
  uint32_t Pm1aControlBlock;
  uint32_t Pm1bControlBlock;
  uint32_t Pm2ControlBlock;
  uint32_t PmTmrBlock;
  uint32_t Gpe0Block;
  uint32_t Gpe1Block;
  uint8_t Pm1EventLength;
  uint8_t Pm1ControlLength;
  uint8_t Pm2ControlLength;
  uint8_t PmTmrLength;
  uint8_t Gpe0Length;
  uint8_t Gpe1Length;
  uint8_t Gpe1Base;
  uint8_t CStateControl;
  uint16_t WorstC2Latency;
  uint16_t WorstC3Latency;
  uint16_t FlushSize;
  uint16_t FlushStride;
  uint8_t DutyOffset;
  uint8_t DutyWidth;
  uint8_t DayAlarm;
  uint8_t MonthAlarm;
  uint8_t Century;
  uint16_t BootArchitectureFlags;
  uint8_t Reserved2;
  uint32_t Flags;
  // ... more fields for ACPI 2.0+
} __attribute__((packed)) FADT;

typedef struct {
  SDTHeader Header;
  uint32_t LocalApicAddress;
  uint32_t Flags;
  uint8_t InterruptControllers[];
} __attribute__((packed)) MADT;

void ACPI_Init(RSDP *rsdp);
void *ACPI_FindTable(const char *signature);

#endif
