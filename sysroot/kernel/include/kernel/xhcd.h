#ifndef USB_H_INCLUDED
#define USB_H_INCLUDED

#include "pci.h"
#include "xhcd-registers.h"

typedef volatile struct{
   uint32_t r0;
   uint32_t r1;
   uint32_t r2;
   uint32_t r3;
}__attribute__((packed))TRB;

typedef volatile struct{
   uint32_t trbPointerLow;
   uint32_t trpPointerHigh;
   uint32_t completionParameter : 24;
   uint32_t completionCode : 8;
   uint32_t cycleBit : 1;
   uint32_t reserved : 9;
   uint32_t trbType : 6;
   uint32_t virtualFunctionId : 8;
   uint32_t slotId : 8;
}__attribute__((packed))CommandCompletionEventTRB;

typedef volatile struct{
   uint32_t ringSegmentLow;
   uint32_t ringSegmentHigh;
   uint32_t reserved : 22;
   uint32_t interrupterTarget : 10;
   uint32_t cycleBit : 1;
   uint32_t toggleCycle : 1;
   uint32_t reserved2 : 2;
   uint32_t chainBit : 1;
   uint32_t interruptOnCompletion : 1;
   uint32_t reserved3 : 4;
   uint32_t trbType : 6;
   uint32_t reserved4 : 16;
}__attribute__((packed))LinkTRB;


typedef volatile struct{
   volatile PciHeader *pciHeader;
   volatile XhciCapabilities *capabilities;
   volatile XhciOperation *operation;
   volatile XhciDoorbell *doorbells;
   volatile InterrupterRegisters* interrupterRegisters;
}__attribute__((packed))Xhci;

typedef volatile struct{
   uint32_t baseAddressLow;
   uint32_t baseAddressHigh;
   uint32_t ringSegmentSize : 16; //p.515. n * 64B
   uint32_t reserved : 16;
   uint32_t reserved2;
}__attribute__((packed))EventRingSegmentTableEntry;


int xhcd_init(PciGeneralDeviceHeader *pciHeader, Xhci *xhci);
int xhcd_checkForDeviceAttach(Xhci *xchi);
int xhcd_isPortEnabled(Xhci *xhci, int portNumber);
int xhcd_enable(Xhci *xhci, int portNumber);
int xhcd_initPort(Xhci *xhci, int portNumber);

#endif
