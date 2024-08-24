#ifndef ACPI_H_INCLUDED
#define ACPI_H_INCLUDED

#include "stdint.h"
#include "stdbool.h"

typedef struct{
   uint8_t id;
   uint32_t address;
   uint32_t globalSystemInterruptBase;
}IoAcpiData;

typedef struct{
   uint8_t acpiProcessorUid;
   uint8_t apicId;
   uint32_t flags;
}LocalApicData;

void acpi_init();

bool acpi_getIOApicData(IoAcpiData *result);
bool acpi_getLocalApicData(LocalApicData *result);

#endif
