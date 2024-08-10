#include "kernel/keyboard.h"
#include "kernel/logging.h"
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
#define REQUEST_SET_IDLE 0xA
#define REQUEST_GET_IDLE 0x2
#define REQUEST_GET_PROTOCOL 0x03

static UsbConfiguration *getConfiguration(UsbDevice *device);
static UsbInterface *getInterface(UsbConfiguration *configuration);
static UsbEndpointDescriptor *getEndpoint(UsbInterface *interface);
static UsbStatus setProtocol(UsbDevice *device, int interface, int protocol);
static UsbStatus setIdleRequest(UsbDevice *device, int interface, uint8_t duration);
static UsbStatus getIdleRequest(UsbDevice *device, int interface, uint8_t *result);
static UsbStatus getReportRequest(UsbDevice *device, int interface, uint8_t *result);

KeyboardStatus keyboard_init(UsbDevice *usbDevice){
   UsbConfiguration *configuration = getConfiguration(usbDevice);
   if(configuration == 0){
      return KeyboardInvalidConfiguration;
   }
   UsbInterface *interface  = getInterface(configuration);
   UsbEndpointDescriptor *endpoint = getEndpoint(interface);


   if(usb_setConfiguration(usbDevice, configuration) != StatusSuccess){
      return KeyboardConfigureError;
   }
   int interfaceNumber = interface->descriptor.bInterfaceNumber;
   uint8_t p1 = 3;
   if(getReportRequest(usbDevice, interfaceNumber, &p1) != StatusSuccess){
      loggError("Unable to get protocol");
   }
   loggDebug("boot protocol? : %b", p1 == BOOT_PROTOCOL);
   if(setProtocol(usbDevice, interfaceNumber, BOOT_PROTOCOL) != StatusSuccess){
      return KeyboardProtocolError;
   }
//    uint8_t idleRate = 0;
//    if(getIdleRequest(usbDevice, interfaceNumber, &idleRate) != StatusSuccess){
//       printf("Could not get idle request\n");
//    }
//    printf("idlerate %d\n", idleRate);
//    if(setIdleRequest(usbDevice, interfaceNumber, 0xFF) != StatusSuccess){
//       printf("Could not set idle request\n");
//    }
   
    uint8_t protocol = 3;
    if(getReportRequest(usbDevice, interfaceNumber, &protocol) != StatusSuccess){
       loggError("Unable to get protocol");
    }
    loggDebug("boot protocol? : %b", protocol == BOOT_PROTOCOL);

//     uint8_t config;
//     if(usb_getConfiguration(usbDevice, &config) != StatusSuccess){
//        printf("U1\n");
//     }
//     printf("config: %d\n", config);

//     uint8_t status[2] = {69, 69};
//     if(usb_getStatus(usbDevice, RecipientDevice, StatusTypeStandard, 0, status) != StatusSuccess){
//        printf("U2\n");
//     }
//     printf("status: %X %X\n", status[0], status[1]);
   
//     printf("gc: %d\n", configuration->descriptor.bConfigurationValue);
//


   

   uint8_t buffer[8] __attribute__((aligned(16)));
   memset(buffer, 0, sizeof(buffer));
   uint8_t buffer2[8] __attribute__((aligned(16)));
   memset(buffer2, 0, sizeof(buffer2));

   for(int i = 0; i < 10000; i++){
      printf("-\b");
   }

   loggInfo("Listening for keypresses:\n");
   uint8_t last = 0;

//    uint64_t *removeMe = (uint64_t*)0xFEBB7000;
//    while(1){
//       printf("-");
//       printf("\b");

//       if(*removeMe != 0){
//          printf("\n%X\n", *removeMe);
//          while(1);
//       }
//    }
   while(1){
//       for(int i = 0; i < 1; i++){
//          uint8_t endpointStatus[2] = {2,2};
//          if(usb_getStatus(usbDevice, RecipientEndpoint, StatusTypeStandard, 2, endpointStatus) != StatusSuccess){
//             printf("U2\n");
//          }

//          printf("endpoint Status %d: %X %X\n", i, endpointStatus[0], endpointStatus[1]);
//       }
      if(usb_readData(usbDevice, *endpoint, buffer, sizeof(buffer)) == StatusError){
         loggError("Error!\n");
         continue;
      }
//       printf("read\n");
//       for(int i = 0; i < 8 ;i++){
//          uint32_t v = buffer[i];
//          printf("%X, ", v);
//       }
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

static UsbStatus setProtocol(UsbDevice *device, int interface, int protocol){
   UsbRequestMessage request;
   request.bmRequestType = 0x21;
   request.bRequest = REQUEST_SET_PROTOCOL;
   request.wValue = protocol;
   request.wIndex = interface;
   request.wLength = 0;
   return usb_configureDevice(device, request);
}
static UsbStatus setIdleRequest(UsbDevice *device, int interface, uint8_t duration){
   UsbRequestMessage request;
   request.bmRequestType = 0x21;
   request.bRequest = REQUEST_SET_IDLE;
   request.wValue = duration << 8;
   request.wIndex = interface;
   request.wLength = 0;
   return usb_configureDevice(device, request);
}
static UsbStatus getIdleRequest(UsbDevice *device, int interface, uint8_t *result){
   UsbRequestMessage request;
   request.bmRequestType = 0xA1;
   request.bRequest = REQUEST_GET_IDLE;
   request.wValue = 0;
   request.wIndex = interface;
   request.wLength = 1;
   request.dataBuffer = result;
   return usb_configureDevice(device, request);
}
static UsbStatus getReportRequest(UsbDevice *device, int interface, uint8_t *result){
   UsbRequestMessage request;
   request.bmRequestType = 0xA1;
   request.bRequest = REQUEST_GET_PROTOCOL;
   request.wValue = 0;
   request.wIndex = interface;
   request.wLength = 1;
   request.dataBuffer = result;
   return usb_configureDevice(device, request);
}
static UsbConfiguration *getConfiguration(UsbDevice *device){
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
