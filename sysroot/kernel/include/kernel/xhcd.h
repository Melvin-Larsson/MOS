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
   Xhci *xhci;
}XhcDevice;

typedef struct{
   int isDirectionIn;
   uint16_t maxPacketSize;
   uint16_t maxBurstSize;
   uint32_t maxESITPayload;
   uint8_t configurationValue;
   uint8_t interval;
}XhcEndpointConfig;

typedef enum{
   XhcOk = 0,
   XhcEnablePortError,
   XhcSlotIdError,
   XhcAddressDeviceError,
   XhcSetMaxPacketSizeError,
   XhcConfigEndpointError,
   XhcReadDataError,
   XhcSendRequestError,

   XhcNotYetImplemented,
}XhcStatus;

/*
  Initializes the xhcd controller.

  @param pciHeader A pointer to a PciGeneralDeviceHeader with
  information about the Xhci controller.
  @param xhci A pointer to memory where the Xhci object will be
  stored.
  @return XhcOk if successfully initialized the controller. Otherwise,
  an error code will be returned.
 */
XhcStatus xhcd_init(const PciGeneralDeviceHeader *pciHeader, Xhci *xhci);
/*
   Send an USB request message.

   @param device The device to send the message to.
   @param request The message to send.
   @return XhcOk if successful. An error code otherwise.
  
 */
XhcStatus xhcd_sendRequest(const XhcDevice *device, UsbRequestMessage request);
/*
  Reads data from an USB endpoint.

  @param device The device to read data from.
  @param endpoint The endpoint to read data from.
  @param dataBuffer A pointer to memory where the result will be put.
  @param bufferSize The size of the dataBuffer.
  @return XhcOk if successful. An error code otherwise.

 */
XhcStatus xhcd_readData(const XhcDevice *device, int endpoint, void *dataBuffer, uint16_t bufferSize);
/*
  Detects and initializes devices that have not been initialized.

  @param xhci The xhc controller to detect devices on.
  @param resultBuffer Memory where information about the devices will
  be stored.
  @param bufferSize The size of resultBuffer.
  @return The number of newly initialized devices.
*/
int xhcd_getDevices(Xhci *xhci, XhcDevice *resultBuffer, int bufferSize);
/** Sets the configuration for an USB device.
 *
 * @param device The device to configure.
 * @param configuration The configuration to use.
 * @return XhcOk if the configuration was successful.
 *         An error code otherwise.
 *
 */
XhcStatus xhcd_setConfiguration(XhcDevice *device, const UsbConfiguration *configuration);

//TODO: Implement:
int xhcd_writeData(const XhcDevice *device, int endpoint, void *dataBuffer, uint16_t bufferSize);

#endif
