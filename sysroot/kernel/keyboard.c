#include "kernel/keyboard.h"
#include "stdio.h"
#include "stdlib.h"

#define BOOT_PROTOCOL 0
#define REPORT_PROTOCOL 1

#define TRANSFER_TYPE_INTERRUPT 0b11
#define DIRECTION_IN 1

#define DIRECTION_BITMASK (1<<7)
#define TRANSFER_TYPE_BITMASK 0b11
#define ENDPOINT_NUMBER_BITMASK 0xF

#define DIRECTION_POS 7
#define TRANSFER_TYPE_POS 0

int keyboard_init(Xhci *xhci, UsbDevice *device){
   UsbConfiguration *config = device->configuration;
   UsbConfigurationDescriptor descriptor = config->descriptor;
   UsbInterface *interface = 0;

   for(int i = 0; i < descriptor.bNumInterfaces; i++){
      UsbInterface *currInterface = &config->interfaces[i];
      printf("class: %X, sub: %X, interface: %X\n",
            currInterface->descriptor.bInterfaceClass,
            currInterface->descriptor.bInterfaceSubClass,
            currInterface->descriptor.bInterfaceProtocol);
      if(currInterface->descriptor.bInterfaceClass == 3 &&
         currInterface->descriptor.bInterfaceSubClass == 1 &&
         currInterface->descriptor.bInterfaceProtocol == 1){
         interface = currInterface;
         //break;
      }
   }
   if(!interface){
      printf("[keyboard] Unable to find valid interface\n");
      return 0;
   }


   UsbEndpointDescriptor *interruptEndpoint = 0;
   printf("endpoint count: %d\n", interface->descriptor.bNumEndpoints);
   for(int i = 0; i < interface->descriptor.bNumEndpoints; i++){
      UsbEndpointDescriptor *endpoint = &interface->endpoints[i];
      printf("addr: %X\n", endpoint->bEndpointAddress);
      if((endpoint->bEndpointAddress & DIRECTION_BITMASK) >> DIRECTION_POS != DIRECTION_IN){
         continue;
      }
      if((endpoint->bmAttributes & TRANSFER_TYPE_BITMASK) >> TRANSFER_TYPE_POS != TRANSFER_TYPE_INTERRUPT){
         continue;
      }
      interruptEndpoint = endpoint;
      break;
   }
   if(interruptEndpoint == 0){
      printf("[keyboard] unable to find interrupt endpoint\n");
      return 0;
   }
   printf("[keyboard] found interrupt endpoint\n");

   if(!xhcd_setConfiguration(xhci, device, config)){
      return 0;
   }
   printf("[keyboard] sucessfully set configuration\n");

   uint8_t interval = interruptEndpoint->bInterval;
   uint8_t address = interruptEndpoint->bEndpointAddress & ENDPOINT_NUMBER_BITMASK;
   uint16_t maxPacketSize = interruptEndpoint->wMaxPacketSize & 0x07FF;
   uint16_t maxBurstSize = (interruptEndpoint->wMaxPacketSize & 0x1800) >> 11;
   printf("interval %d\n", interval);

   XhcEndpointConfig endpointConfig;
   endpointConfig.isDirectionIn = 1;
   endpointConfig.maxPacketSize = maxPacketSize;
   endpointConfig.maxBurstSize = maxBurstSize;
   endpointConfig.maxESITPayload = maxPacketSize * (maxBurstSize + 1);
   endpointConfig.configurationValue = descriptor.bConfigurationValue;
   endpointConfig.interval = interval;

   if(!xhcd_initInterruptEndpoint(xhci, device, address, endpointConfig)){
      printf("[keyboard] Failed to init interrupt endpoint\n");
      return 0;
   }
   printf("[keyboard] initialized interrupt endpoint\n");

   volatile uint64_t *dcAddressArray = xhci->dcBaseAddressArray;
   uintptr_t ptr = dcAddressArray[device->slotId];
   XhcOutputContext *outputContext = (XhcOutputContext*)ptr;
   XhcEndpointContext endpoint = outputContext->endpointContext[address * 2 + 1 - 1];
   printf("endpoint state %X\n", endpoint.endpointState);
   printf("endpoint interval %d\n", endpoint.interval);
   printf("avg trb length %d\n", endpoint.avarageTrbLength);
   printf("max ESIs %X\n", endpoint.maxESITPayloadLow);

   
   
   int interfaceNumber = interface->descriptor.bInterfaceNumber;
   if(!xhcd_setProtocol(xhci, device, interfaceNumber, BOOT_PROTOCOL)){
      return 0;
   }
   printf("[keyboard] sucessfully set boot protocol\n");

   
   XhcEventTRB result;
   int index = address * 2 + 1;
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


   return 1;
}

