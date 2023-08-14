#ifndef USB_H_INCLUDED
#define USB_H_INCLUDED

#include "pci.h"
#include "xhcd-registers.h"
#include "xhcd-ring.h"
#include "xhcd-event-ring.h"
#include "usb-descriptors.h"

typedef struct{
   PciHeader *pciHeader;
   XhciCapabilities *capabilities;
   XhciOperation *operation;
   XhciDoorbell *doorbells;
   InterrupterRegisters* interrupterRegisters;

   volatile uint64_t *dcBaseAddressArray;
   XhcdRing transferRing[16 + 1][31]; //indexed from 1 //FIXME
   XhcEventRing eventRing;
   XhcdRing commandRing;
}Xhci;

typedef struct{
   int slotId;
   UsbConfiguration *configuration;
}UsbDevice;


int xhcd_init(PciGeneralDeviceHeader *pciHeader, Xhci *xhci);
int xhcd_checkForDeviceAttach(Xhci *xchi);
int xhcd_isPortEnabled(Xhci *xhci, int portNumber);
int xhcd_enable(Xhci *xhci, int portNumber);
int xhcd_initPort(Xhci *xhci, int portNumber);

int xhcd_getDeviceDescriptor(
      Xhci *xhci,
      int slotId,
      UsbDeviceDescriptor *result);
UsbConfiguration *xhcd_getConfiguration(
      Xhci *xhci,
      int slotId,
      int configuration
      );
void xhcd_freeConfiguration(UsbConfiguration *config);
void xhcd_freeInterface(UsbInterface *interface);


#endif
