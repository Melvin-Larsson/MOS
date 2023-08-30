#ifndef XHCD_H_INCLUDED
#define XHCD_H_INCLUDED

#include "pci.h"
#include "xhcd-registers.h"
#include "xhcd-ring.h"
#include "xhcd-event-ring.h"
#include "usb-descriptors.h"
#include "usb-messages.h"

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
}XhcDevice;

typedef struct{
   int isDirectionIn;
   uint16_t maxPacketSize;
   uint16_t maxBurstSize;
   uint32_t maxESITPayload;
   uint8_t configurationValue;
   uint8_t interval;
}XhcEndpointConfig;


int xhcd_init(PciGeneralDeviceHeader *pciHeader, Xhci *xhci);
int xhcd_sendRequest(Xhci *xhci, XhcDevice *device, UsbRequestMessage request);
int xhcd_readData(Xhci *xhci, XhcDevice *device, int endpoint, void *dataBuffer, uint16_t bufferSize);
int xhcd_getDevices(Xhci *xhci, XhcDevice *resultBuffer, int bufferSize);
int xhcd_setConfiguration(Xhci *xhci, XhcDevice *device, const UsbConfiguration *configuration);

//TODO: Implement:
int xhcd_writeData(Xhci *xhci, XhcDevice *device, int endpoint, void *dataBuffer, uint16_t bufferSize);

#endif
