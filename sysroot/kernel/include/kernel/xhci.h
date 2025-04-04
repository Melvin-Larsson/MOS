#ifndef XHCI_H_INCLUDED
#define XHCI_H_INCLUDED

#include "stdint.h"
#include "pci.h"
#include "usb-descriptors.h"
#include "usb-messages.h"

typedef enum{
   PortUsbTypeUnknown = 0,
   PortUsbType2 = 2,
   PortUsbType3 = 3,
}PortUsbType;

typedef struct{
   PortUsbType usbType;
   uint8_t protocolSlotType;
}UsbPortInfo;

typedef struct{
   void (*handler)(void *data);
   void *data;
}XhcInterruptHandler;

typedef struct{
   void *data;
}Xhci;

typedef struct{
   uint8_t slotId;
   uint8_t portIndex;
   uint8_t portSpeed;
   void *data;
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
   XhcControllerTimedOut,
   XhcControllerUnexpectedState,
   XhcUnableToAllocateMemory,

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
XhcStatus xhcd_init(PciDescriptor pciHeader, Xhci *xhci);

void xhcd_free(Xhci *xhci);
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
XhcStatus xhcd_readData(const XhcDevice *device, UsbEndpointDescriptor endpoint, void *dataBuffer, uint16_t bufferSize);
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

XhcStatus xhcd_setInterrupter(XhcDevice *device, int endpoint, void (*handler)(void *), void *data);

void xhc_dumpCapabilityRegs(Xhci *xhci);
void xhc_dumpOperationalRegs(Xhci *xhci);

//TODO: Implement:
XhcStatus xhcd_writeData(const XhcDevice *device,
      UsbEndpointDescriptor endpoint,
      void *dataBuffer,
      uint16_t bufferSize);

#endif
