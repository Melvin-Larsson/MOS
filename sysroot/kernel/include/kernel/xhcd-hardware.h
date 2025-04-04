#ifndef XHCD_HARDWARE_H_INCLUDED
#define XHCD_HARDWARE_H_INCLUDED

#include "stdint.h"
#include "pci.h"

#define USBSTS_HCH_BIT (1 << 0)
#define USBSTS_HSE_BIT (1 << 2)
#define USBSTS_EINT_BIT (1 << 3)
#define USBSTS_PCD_BIT (1 << 4)
#define USBSTS_SSS_BIT (1 << 8)
#define USBSTS_RSS_BIT (1 << 9)
#define USBSTS_SRE_BIT (1 << 10)
#define USBSTS_CNR_BIT (1 << 11)
#define USBSTS_HCE_BIT (1 << 12)

typedef enum{
   USBCommand = 0x0,
   USBStatus = 0x4,
   PAGESIZE = 0x8,
   DNCTRL = 0x14,
   CRCR = 0x18,
   DCBAAP = 0x30,
   CONFIG = 0x38,
}XhcOperationalRegister;

typedef enum{
   PORTStatusAndControl = 0x0,
   PORTPMSC = 0x4,
   PORTLI = 0x8,
   PORTHLPMC = 0xC
}XhcPortRegister;

typedef enum{
   CAPLENGTH = 0,
   HCIVERSION = 0x2,
   HCSPARAMS1 = 0x04,
   HCSPARAMS2 = 0x8,
   HCSPARAMS3 = 0xC,
   HCCPARAMS1 = 0x10,
   DBOFF = 0x14,
   RTSOFF = 0x18,
   HCCPARAMS2 = 0x1C,
}XhcCapabilityRegister;

typedef enum{
   IMAN = 0x0,
   IMOD = 0x4,
   ERSTSZ = 0x08,
   ERSTBA = 0x10,
   ERDP = 0x18,
}XhcInterruptorRegister;

typedef struct{
   uintptr_t capabilityBase;
   uintptr_t operationalBase;
   uintptr_t doorbellBase;
   uintptr_t runtimeBase;
}XhcHardware;

typedef struct{
   void *data;
}XhcExtendedCapabilityEnumerator;

XhcHardware xhcd_initRegisters(PciGeneralDeviceHeader pciHeader);
void xhcd_writeRegister(XhcHardware xhcHardware, XhcOperationalRegister xhcRegister, uint64_t data);
void xhcd_orRegister(XhcHardware xhcHardware, XhcOperationalRegister xhcdRegister, uint32_t orValue);
void xhcd_andRegister(XhcHardware xhcHardware, XhcOperationalRegister xhcdRegister, uint32_t andValue);

uint64_t xhcd_readRegister(XhcHardware xhcHardware, XhcOperationalRegister xhcRegister);
void xhcd_writePortRegister(XhcHardware xhcHardware, uint32_t port, XhcPortRegister XhcPortRegister, uint32_t data);
uint32_t xhcd_readPortRegister(XhcHardware xhcHardware, uint32_t port, XhcPortRegister portRegister);

uint32_t xhcd_readCapability(XhcHardware xhcHardware, XhcCapabilityRegister capabilityRegister);
void xhcd_writeCapability(XhcHardware xhcHardware, XhcCapabilityRegister capabilityRegister, uint32_t data);

void xhcd_writeInterrupter(XhcHardware xhcHardware, uint16_t index, XhcInterruptorRegister interruptorRegister, uint64_t value);
uint64_t xhcd_readInterrupter(XhcHardware xhcHardware, uint8_t index, XhcInterruptorRegister interruptorRegister);
void xhcd_orInterrupter(XhcHardware xhc, uint16_t index, XhcInterruptorRegister interruptorRegister, uint64_t orValue);
void xhcd_andInterrupter(XhcHardware xhc, uint16_t index, XhcInterruptorRegister interruptorRegister, uint64_t andValue);

void xhcd_writeDoorbell(XhcHardware xhcHardware, uint8_t index, uint32_t value);
uint32_t xhcd_readDoorbell(XhcHardware xhcHardware, uint8_t index);

XhcExtendedCapabilityEnumerator xhcd_newExtendedCapabilityEnumerator(XhcHardware xhc);
void xhcd_advanceExtendedCapabilityEnumerator(XhcExtendedCapabilityEnumerator *enumerator);
void xhcd_readExtendedCapability(XhcExtendedCapabilityEnumerator *enumerator, void *result, int size);
void xhcd_writeExtendedCapability(XhcExtendedCapabilityEnumerator *enumerator, void *data, int size);
int xhcd_hasNextExtendedCapability(XhcExtendedCapabilityEnumerator *enumerator);

#endif
