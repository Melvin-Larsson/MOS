#include "kernel/usb-mass-storage.h"
#include "kernel/usb.h"

#define CLASS_MASS_STORAGE 0x08
#define SUBCLASS_SCSI 0x06
#define PROTOCOL_BULK_ONLY 0x50


#define CBW_SIGNATURE 0x43425355
#define CBW_DATA_IN 1

typedef struct{
   uint32_t dCBWSignature;
   uint32_t dCBWTag;
   uint32_t DCBWDataTransferLength;
   uint8_t bmCBWFlags;
   uint8_t bCBWLUN : 4;
   uint8_t reserved1 : 4;
   uint8_t bCBWCBLength : 5;
   uint8_t reserved2 : 3;
   uint8_t CBWCB[16];
}__attribute__((packed))CBW;

typedef struct{
   uint8_t opcode;
   uint8_t parameters[15];

}__attribute__((packed))SCICommand;

static UsbConfiguration *getConfiguration(UsbDevice *device);
static UsbInterface *getInterface(UsbConfiguration *configuration);
static UsbEndpointDescriptor *getBulkInEndpoint(UsbInterface *interface);

static UsbMassStorageStatus bulkOnlyMassStorageReset(UsbMassStorageDevice *device);

UsbMassStorageStatus usbMassStorage_init(UsbDevice *usbDevice, UsbMassStorageDevice *result){
   UsbConfiguration *config = getConfiguration(usbDevice);
   if(!config){
      return UsbMassStorageInvalidDevice;
   }
   UsbInterface *interface = getInterface(config);
   if(!interface){
      return UsbMassStorageInvalidDevice;
   }
   UsbEndpointDescriptor *bulkInEndpoint = getBulkInEndpoint(interface);
   if(!bulkInEndpoint){
      return UsbMassStorageInvalidDevice;
   }
   if(usb_setConfiguration(usbDevice, config) != StatusSuccess){
      return UsbMassStorageConfigError;
   }

   *result = (UsbMassStorageDevice){
      .usbDevice = usbDevice,
      .configuration = config,
      .interface = interface,
   };

   return UsbMassStorageSuccess;
}

static UsbMassStorageStatus bulkOnlyMassStorageReset(UsbMassStorageDevice *device){
   UsbRequestMessage message;
   message.bmRequestType = 0b00100001;
   message.bRequest = 0xFF;
   message.wValue = 0;
   message.wIndex = device->interface->descriptor.bInterfaceNumber;
   message.wLength = 0;

   if(usb_configureDevice(device->usbDevice, message) != StatusSuccess){
      return UsbMassStorageConfigError;
   }
   return UsbMassStorageSuccess;
}


static UsbConfiguration *getConfiguration(UsbDevice *device){
   for(int i = 0; i < device->configurationCount; i++){
      UsbConfiguration *config = &device->configuration[i];
      if(getInterface(config)){
         return config;
      }
   }
   return 0;
}
static UsbInterface *getInterface(UsbConfiguration *configuration){
   for(int i = 0; i < configuration->descriptor.bNumInterfaces; i++){
      UsbInterface *interface = &configuration->interfaces[i];
      UsbInterfaceDescriptor descriptor = interface->descriptor;
      if(descriptor.bInterfaceClass != CLASS_MASS_STORAGE){
         continue;
      }
      if(descriptor.bInterfaceSubClass != SUBCLASS_SCSI){
         continue;
      }
      if(descriptor.bInterfaceProtocol != PROTOCOL_BULK_ONLY){
         continue;
      }
      return interface;
   }
   return 0;
}
static UsbEndpointDescriptor *getBulkInEndpoint(UsbInterface *interface){
   for(int i = 0; i < interface->descriptor.bNumEndpoints; i++){
      if(interface->endpoints[i].direction == ENDPOINT_DIRECTION_IN){
         return &interface->endpoints[i];
      }
   }
   return 0;
}


