#include "kernel/keyboard.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "kernel/usb.h"

#define BOOT_PROTOCOL 0
#define REPORT_PROTOCOL 1

#define TRANSFER_TYPE_INTERRUPT 0b11
#define DIRECTION_IN 1

#define DIRECTION_BITMASK (1<<7)
#define TRANSFER_TYPE_BITMASK 0b11
#define ENDPOINT_NUMBER_BITMASK 0xF

#define DIRECTION_POS 7
#define TRANSFER_TYPE_POS 0

static UsbConfiguration *getConfiguration(UsbDevice2 *device);
static UsbInterface *getInterface(UsbConfiguration *configuration);
static UsbEndpointDescriptor *getEndpoint(UsbInterface *interface);
static int setProtocol(UsbDevice2 *device);

KeyboardStatus keyboard_init(Xhci *xhci, UsbDevice *device){
   Usb usb = {xhci};
   UsbDevice2 usbDevice = {device->slotId, device->configuration, 1, &usb};

   UsbConfiguration *configuration = getConfiguration(&usbDevice);
   if(configuration == 0){
      return KeyboardInvalidConfiguration;
   }
   UsbInterface *interface  = getInterface(configuration);
   UsbEndpointDescriptor *endpoint = getEndpoint(interface);

   if(usb_setConfiguration(&usbDevice, usbDevice.configuration) != StatusSuccess){
      return KeyboardConfigureError;
   }
   int interfaceNumber = interface->descriptor.bInterfaceNumber;
   if(!xhcd_setProtocol(xhci, device, interfaceNumber, BOOT_PROTOCOL)){
      return KeyboardProtocolError;
   }

   XhcEventTRB result;
   int index = endpoint->endpointNumber * 2 + 1;
   XhcdRing transferRing = xhci->transferRing[device->slotId][index - 1];
   uint8_t buffer[8];
   memset(buffer, 0, sizeof(buffer));
   for(int i = 0; i < 14; i++){
      xhcd_putTRB(TRB_NORMAL(buffer, sizeof(buffer)), &transferRing);
   }
   xhcd_ringDoorbell(xhci, device->slotId, index);

   printf("listening for keypresses:\n");
   uint8_t last = 0;
   while(1){
      while(!xhcd_readEvent(&xhci->eventRing, &result, 1));
      xhcd_putTRB(TRB_NORMAL(buffer, sizeof(buffer)), &transferRing);
      if(result.completionCode != Success){
         printf("[keyboard] failed event\n");
         return 0;
      }
      for(int i = 0; i < 6 && buffer[2] != last; i++){
         uint8_t keypress = buffer[2 + i];
         if(keypress == 0){
            break;
         }
         if(keypress == 0x2c){
            printf(" ");
         }
         else if(keypress == 0x28){
            printf("\n");
         }
         else if(keypress == 0x2a){
            printf("\b");
         }
         else{
            uint8_t character = 'a' + keypress - 4;
            uint8_t str[] = {character, 0};
            printf("%s", str);
         }
      }
      last = buffer[2];
   }


   return KeyboardSuccess;
}

void keyboard_getStatusCode(KeyboardStatus status, char output[100]){
   char *statusCodes[] = {
      "Success",
      "Unable to find valid configuration",
      "Failed to configure keyboard",
      "Failed to set protocol"
   };
   strcpy(output, "[keyboard] ");
   strAppend(output, statusCodes[status]);
}
static UsbConfiguration *getConfiguration(UsbDevice2 *device){
   for(int i = 0; i < device->configurationCount; i++){
      if(getInterface(&device->configuration[i]) != 0){
         return &device->configuration[i];
      }
   }
   return 0;
}
static UsbInterface *getInterface(UsbConfiguration *configuration){
   for(int i = 0; i < configuration->descriptor.bNumInterfaces; i++){
      UsbInterfaceDescriptor *interface = &configuration->interfaces[i].descriptor;
      if(interface->bInterfaceClass != 3){
         continue;
      }
      if(interface->bInterfaceSubClass != 1){
         continue;
      }
      if(interface->bInterfaceProtocol != 1){
         continue;
      }
      if(getEndpoint(&configuration->interfaces[i]) != 0){
         return &configuration->interfaces[i];
      }

   }
   return 0;
}
static UsbEndpointDescriptor *getEndpoint(UsbInterface *interface){
   UsbInterfaceDescriptor *descriptor = &interface->descriptor;
   for(int i = 0; i < descriptor->bNumEndpoints; i++){
      UsbEndpointDescriptor *endpoint = &interface->endpoints[i];
       if((endpoint->bEndpointAddress & DIRECTION_BITMASK) >> DIRECTION_POS != DIRECTION_IN){
         continue;
      }
      if((endpoint->bmAttributes & TRANSFER_TYPE_BITMASK) >> TRANSFER_TYPE_POS != TRANSFER_TYPE_INTERRUPT){
         continue;
      }
      return &interface->endpoints[i];
   }
   return 0;
}
