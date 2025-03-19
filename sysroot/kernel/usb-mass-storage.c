#include "kernel/usb-mass-storage.h"
#include "kernel/usb.h"
#include "kernel/scsi.h"
#include "kernel/logging.h"
#include "string.h"
#include "stdlib.h"
#define CLASS_MASS_STORAGE 0x08
#define SUBCLASS_SCSI 0x06
#define PROTOCOL_BULK_ONLY 0x50

#define SIGNATURE_CBW 0x43425355
#define SIGNATURE_CSW 0x53425355

#define CBW_DATA_IN 1

#define REQUEST_TYPE_RESET 0b00100001

typedef enum{
   directionOut = 0,
   directionIn = 1,
}CbwDirection;

typedef enum{
   commandPassed = 0,
   commandFailed = 1,
   phaseError = 2,
}CswStatus;

typedef struct{
   uint32_t signature;
   uint32_t tag;
   uint32_t dataTransferLength;
   union{
      uint8_t flags;
      struct{
         uint8_t reserved : 6;
         uint8_t obsolete : 1;
         uint8_t direction : 1;
      };
   };
   uint8_t lun : 4;
   uint8_t reserved1 : 4;
   uint8_t cbLength : 5;
   uint8_t reserved2 : 3;
   uint8_t cb[16];
}__attribute__((packed))CBW;

typedef struct{
   uint32_t signature;
   uint32_t tag;
   uint32_t dataResidue;
   CswStatus status : 8;
}__attribute__((packed))CSW;


static UsbMassStorageStatus readInquiryData(UsbMassStorageDevice *device);
static UsbMassStorageStatus testUnitReady(const UsbMassStorageDevice *device);
static UsbMassStorageStatus readCapacity(UsbMassStorageDevice *device);
static UsbMassStorageStatus readStatus(const UsbMassStorageDevice *device);
static UsbMassStorageStatus bulkOnlyMassStorageReset(UsbMassStorageDevice *device);

static UsbConfiguration *getConfiguration(UsbDevice *device);
static UsbInterface *getInterface(UsbConfiguration *configuration);
static UsbEndpointDescriptor *getBulkInEndpoint(UsbInterface *interface);
static UsbEndpointDescriptor *getBulkOutEndpoint(UsbInterface *interface);

static CBW newCBW(uint8_t *cbwcb, uint8_t cbwcbLength);

UsbMassStorageStatus usbMassStorage_init(UsbDevice *usbDevice, UsbMassStorageDevice *result){
   UsbConfiguration *config = getConfiguration(usbDevice);
   if(!config){
      loggError("nocfg\n");
      return UsbMassStorageInvalidDevice;
   }
   UsbInterface *interface = getInterface(config);
   if(!interface){
      loggError("noif\n");
      return UsbMassStorageInvalidDevice;
   }
   UsbEndpointDescriptor *bulkInEndpoint = getBulkInEndpoint(interface);
   UsbEndpointDescriptor *bulkOutEndpoint = getBulkOutEndpoint(interface);
   if(!bulkInEndpoint || !bulkOutEndpoint){
      loggError("nobulkin/bulkout\n");
      return UsbMassStorageInvalidDevice;
   }
   if(usb_setConfiguration(usbDevice, config) != StatusSuccess){
      loggError("failedconfig\n");
      return UsbMassStorageConfigError;
   }

   *result = (UsbMassStorageDevice){
      .usbDevice = usbDevice,
      .configuration = config,
      .interface = interface,
      .bulkOutEndpoint = *bulkOutEndpoint,
      .bulkInEndpoint = *bulkInEndpoint
   };

   loggInfo("1");
   UsbMassStorageStatus s1 = bulkOnlyMassStorageReset(result);
   if(s1 != UsbMassStorageSuccess) {
      loggInfo("r1");
      return s1;
   }

   loggInfo("2");
   UsbMassStorageStatus s2 = readInquiryData(result);
   if(s2 != UsbMassStorageSuccess) {
      loggInfo("r2");
      return s2;
   }

   loggInfo("3");
   UsbMassStorageStatus s3 = testUnitReady(result);
   if(s3 != UsbMassStorageSuccess) {
      loggInfo("r3");
      return s3;
   }

   loggInfo("4");
   UsbMassStorageStatus s4 = readCapacity(result);
   if(s4 != UsbMassStorageSuccess) {
      loggInfo("r4");
      return s4;
   }
   loggInfo("Device has capacity %d", result->capacity);

   return UsbMassStorageSuccess;
}

UsbMassStorageStatus usbMassStorage_read(const UsbMassStorageDevice *device,
      uint32_t logicalBlockAddress,
      void * resultBuffer,
      uint32_t bufferSize){

   if(bufferSize == 0){
      return UsbMassStorageSuccess;
   }

   uint32_t blockCount = (bufferSize + device->capacity - 1)/device->capacity;

   if(logicalBlockAddress + blockCount > device->maxLogicalBlockAddress + 1){
      return UsbMassStorageInvalidAddress;
   }

   ScsiCDB readCdb = Scsi_CDB_READ10(logicalBlockAddress, blockCount);
   CBW readCBW = newCBW(readCdb.bytes, readCdb.size);
   readCBW.dataTransferLength = bufferSize;
   readCBW.direction = directionIn;

   usb_writeData(device->usbDevice, device->bulkOutEndpoint, &readCBW, sizeof(CBW));

   usb_readData(device->usbDevice, device->bulkInEndpoint, resultBuffer, bufferSize);

   return readStatus(device);
}
UsbMassStorageStatus usbMassStorage_write(const UsbMassStorageDevice *device,
      uint32_t logicalBlockAddress,
      void *data,
      uint32_t dataSize){

   if(dataSize == 0){
      return UsbMassStorageSuccess;
   }

   uint32_t blockCount = (dataSize + device->capacity - 1)/device->capacity;

   if(logicalBlockAddress + blockCount > device->maxLogicalBlockAddress + 1){
      return UsbMassStorageInvalidAddress;
   }

   ScsiCDB writeCdb = Scsi_CDB_WRITE10(logicalBlockAddress, blockCount);
   CBW writeCBW = newCBW(writeCdb.bytes, writeCdb.size);
   writeCBW.dataTransferLength = dataSize;
   writeCBW.direction = directionOut;

   usb_writeData(device->usbDevice, device->bulkOutEndpoint, &writeCBW, sizeof(CBW));
   usb_writeData(device->usbDevice, device->bulkOutEndpoint, data, dataSize);

   return readStatus(device);
}


static UsbMassStorageStatus readInquiryData(UsbMassStorageDevice *device){
   ScsiCDB inquiryCdb = Scsi_CDB_INQUIRY(0, sizeof(device->inquiryData));
   CBW inquiryCBW = newCBW(inquiryCdb.bytes, inquiryCdb.size);
   inquiryCBW.dataTransferLength = sizeof(device->inquiryData);
   inquiryCBW.direction = directionIn;
   usb_writeData(device->usbDevice, device->bulkOutEndpoint, &inquiryCBW, sizeof(CBW));

   usb_readData(device->usbDevice, device->bulkInEndpoint, &device->inquiryData, sizeof(device->inquiryData));

   char str[sizeof(device->inquiryData) + 1];
   str[sizeof(device->inquiryData)] = 0;
   memcpy(&str, (char*)&device->inquiryData + 8, sizeof(device->inquiryData));
   loggInfo("%s", str);

   return readStatus(device);
}
static UsbMassStorageStatus testUnitReady(const UsbMassStorageDevice *device){
   UsbMassStorageStatus status;
   for(int i = 0; i < 3; i++){
      ScsiCDB testUnitReadyCdb = Scsi_CDB_TestUnitReady();
      CBW testUnitReadyCBW = newCBW(testUnitReadyCdb.bytes, testUnitReadyCdb.size);
      usb_writeData(device->usbDevice, device->bulkOutEndpoint, &testUnitReadyCBW, sizeof(CBW));
      status = readStatus(device);
      if(status == UsbMassStorageSuccess){
         break;
      }
   }
   return status;
}
static UsbMassStorageStatus readCapacity(UsbMassStorageDevice *device){
   uint8_t capacityBuffer[8];

   ScsiCDB readCapacityCdb = Scsi_CDB_ReadCapacity();
   CBW readCapacityCBW = newCBW(readCapacityCdb.bytes, readCapacityCdb.size);
   readCapacityCBW.dataTransferLength = sizeof(capacityBuffer);
   readCapacityCBW.direction = directionIn;

   usb_writeData(device->usbDevice, device->bulkOutEndpoint, &readCapacityCBW, sizeof(CBW));
   usb_readData(device->usbDevice, device->bulkInEndpoint, capacityBuffer, sizeof(capacityBuffer));

   UsbMassStorageStatus status = readStatus(device);

   if(status == UsbMassStorageSuccess){
      uint32_t *maxLogicalBlockAddress = (uint32_t*)&capacityBuffer;
      uint32_t *capacity = (uint32_t*)(capacityBuffer + 4);

      device->maxLogicalBlockAddress = __builtin_bswap32(*maxLogicalBlockAddress);
      device->capacity = __builtin_bswap32(*capacity);
   }
   return status;
}

static UsbMassStorageStatus readStatus(const UsbMassStorageDevice *device){
   CSW status;

   usb_readData(device->usbDevice, device->bulkInEndpoint, &status, sizeof(CSW));

   if(status.signature != SIGNATURE_CSW){
      uint8_t *bytes = (uint8_t *)&status;
      char buffer[sizeof(CSW) * (sizeof("0xFF, ") - 1) + 1];
      memset(buffer, 0, sizeof(buffer));
      char *buffer_pointer = buffer;
      for(size_t i = 0; i < sizeof(CSW); i++){
         buffer_pointer += sprintf(buffer_pointer, "%X, ", bytes[i]);
      }
      loggDebug("%s", buffer);

      return UsbMassStorageUnexpectedMessage;
   }

   if(status.status == commandPassed){
      return UsbMassStorageSuccess;
   }
   if(status.status == phaseError){
      return UsbMassStoragePhaseError;
   }
   return UsbMassStorageCommandFailed;
}

static UsbMassStorageStatus bulkOnlyMassStorageReset(UsbMassStorageDevice *device){
   UsbRequestMessage message = (UsbRequestMessage){
      .bmRequestType = REQUEST_TYPE_RESET,
      .bRequest = 0xFF,
      .wValue = 0,
      .wIndex = device->interface->descriptor.bInterfaceNumber,
      .wLength = 0,
   };
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
      loggDebug("Interface %d, subclass %d, protocal %d", descriptor.bInterfaceClass, descriptor.bInterfaceSubClass, descriptor.bInterfaceProtocol);
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
static UsbEndpointDescriptor *getBulkOutEndpoint(UsbInterface *interface){
   for(int i = 0; i < interface->descriptor.bNumEndpoints; i++){
      if(interface->endpoints[i].direction == ENDPOINT_DIRECTION_OUT){
         return &interface->endpoints[i];
      }
   }
   return 0;
}


static CBW newCBW(uint8_t *cbwcb, uint8_t cbwcbLength){
   CBW cbw = (CBW){
      .signature = SIGNATURE_CBW,
      .tag = 0,
      .dataTransferLength = 0,
      .reserved = 0,
      .direction = directionOut,
      .lun = 0,
      .cbLength = cbwcbLength,
   };
   memcpy(&cbw.cb, cbwcb, cbwcbLength);
   return cbw;
}
