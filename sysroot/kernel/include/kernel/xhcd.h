#ifndef XHCD_H_INCLUDED
#define XHCD_H_INCLUDED

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

typedef struct{
   int isDirectionIn;
   uint16_t maxPacketSize;
   uint16_t maxBurstSize;
   uint32_t maxESITPayload;
   uint8_t configurationValue;
   uint8_t interval;
}XhcEndpointConfig;


int xhcd_init(PciGeneralDeviceHeader *pciHeader, Xhci *xhci);
int xhcd_getNewlyAttachedDevices(Xhci *xhci, uint32_t *resultBuffer, int bufferSize);
int xhcd_isPortEnabled(Xhci *xhci, int portNumber);
int xhcd_initPort(Xhci *xhci, int portNumber);
int xhcd_initInterruptEndpoint(Xhci *xhci, int slotId, UsbEndpointDescriptor *endpoint);
int xhcd_configureEndpoint(Xhci *xhci, int slotId, UsbEndpointDescriptor *endpoint);

int xhcd_getDeviceDescriptor(
      Xhci *xhci,
      int slotId,
      UsbDeviceDescriptor *result);
UsbConfiguration *xhcd_getConfiguration(
      Xhci *xhci,
      int slotId,
      int configuration
      );
int xhcd_readData(Xhci *xhci, int slotId, int endpoint, void *dataBuffer, uint16_t bufferSize);
int xhcd_setConfiguration(Xhci *xhci, int slotId, UsbConfiguration *configuration);
int xhcd_setProtocol(Xhci *xhci, UsbDevice *device, int interface, int protocol);
void xhcd_freeConfiguration(UsbConfiguration *config);
void xhcd_freeInterface(UsbInterface *interface);

void xhcd_ringDoorbell(Xhci *xhci, uint8_t slotId, uint8_t target);


#endif
