#include "kernel/keyboard.h"
#include "stdio.h"

int keyboard_init(Xhci *xhci, UsbDevice *device){
   UsbConfiguration *config = device->configuration;
   UsbConfigurationDescriptor descriptor = config->descriptor;
   if(descriptor.bNumInterfaces != 1){
      printf("[keyboard] unable to pick interface\n");
      return -1;
   }
   UsbInterface *interface = &config->interfaces[0];
   for(int i = 0; i < interface->descriptor.bNumEndpoints; i++){
      UsbEndpointDescriptor endpoint = interface->endpoints[i];
      printf("endpoint %X\n", endpoint.bDescriptorType);
   }
   



}

