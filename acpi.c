#include "acpi.h"
#include "libc.h"

static RSDP *g_rsdp = NULL;
static XSDT *g_xsdt = NULL;

void ACPI_Init(RSDP *rsdp) {
  g_rsdp = rsdp;
  if (rsdp->Revision >= 2) {
    g_xsdt = (XSDT *)rsdp->XsdtAddress;
  }
}

void *ACPI_FindTable(const char *signature) {
  if (!g_xsdt)
    return NULL;

  int num_tables = (g_xsdt->Header.Length - sizeof(SDTHeader)) / 8;
  for (int i = 0; i < num_tables; i++) {
    SDTHeader *header = (SDTHeader *)g_xsdt->Tables[i];
    if (memcmp(header->Signature, signature, 4) == 0) {
      return header;
    }
  }

  return NULL;
}
