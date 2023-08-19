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

#define REQUEST_SET_PROTOCOL 0xB

static UsbConfiguration *getConfiguration(UsbDevice2 *device);
static UsbInterface *getInterface(UsbConfiguration *configuration);
static UsbEndpointDescriptor *getEndpoint(UsbInterface *interface);
static UsbStatus setProtocol(UsbDevice2 *device, int interface, int protocol);

KeyboardStatus keyboard_init(UsbDevice2 *usbDevice){

   UsbConfiguration *configuration = getConfiguration(usbDevice);
   if(configuration == 0){
      return KeyboardInvalidConfiguration;
   }
   UsbInterface *interface  = getInterface(configuration);
   UsbEndpointDescriptor *endpoint = getEndpoint(interface);

   if(usb_setConfiguration(usbDevice, usbDevice->configuration) != StatusSuccess){
      return KeyboardConfigureError;
   }
   int interfaceNumber = interface->descriptor.bInterfaceNumber;
   if(setProtocol(usbDevice, interfaceNumber, BOOT_PROTOCOL) != StatusSuccess){
      return KeyboardProtocolError;
   }

   int index = endpoint->endpointNumber * 2 + 1;
   uint8_t buffer[8];
   memset(buffer, 0, sizeof(buffer));

   printf("listening for keypresses:\n");
   uint8_t last = 0;
   while(1){
      usb_readData(usbDevice, index, buffer, sizeof(buffer));
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

static UsbStatus setProtocol(UsbDevice2 *device, int interface, int protocol){
   UsbRequestMessage request;
   request.bmRequestType = 0x21;
   request.bRequest = REQUEST_SET_PROTOCOL;
   request.wValue = protocol;
   request.wIndex = interface;
   request.wLength = 0;
   return usb_configureDevice(device, request);
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
