#ifndef USB_DESCRIPTORS_H_INCLUDED
#define USB_DESCRIPTORS_H_INCLUDED

#include "stdint.h"

#define ENDPOINT_DIRECTION_OUT 0
#define ENDPOINT_DIRECTION_IN 1

#define ENDPOINT_TRANSFER_TYPE_CONTROL 0
#define ENDPOINT_TRANSFER_TYPE_ISOCHRONOUS 1 
#define ENDPOINT_TRANSFER_TYPE_BULK 2
#define ENDPOINT_TRANSFER_TYPE_INTERRUPT 3

#define ENDPOINT_INTERRUPT_USAGE_TYPE_PERIODIC 0
#define ENDPOINT_INTERRUPT_USAGE_TYPE_NOTIFICATION 1

#define ENDPOINT_ISOCHRONOUS_USAGE_TYPE_DATA 0
#define ENDPOINT_ISOCHRONOUS_USAGE_TYPE_FEEDBACK 1
#define ENDPOINT_ISOCHRONOUS_USAGE_TYPE_IMPLICIT_FEEDBACK 2

typedef struct{
   uint8_t bLength;
   uint8_t bDescriptorType;
   uint16_t bcd;
   uint8_t bDeviceClass;
   uint8_t bDeviceSubClass;
   uint8_t bDeviceProtocol;
   uint8_t bMaxPacketSize0;
   uint16_t idVendor;
   uint16_t idProduct;
   uint16_t bcdDevice;
   uint8_t iManufacturer;
   uint8_t iProduct;
   uint8_t iSerialNumber;
   uint8_t bNumConfigurations;
}__attribute__((packed))UsbDeviceDescriptor;

typedef struct{
   uint8_t bLength;
   uint8_t bDescriptorType;
   uint16_t wTotalLenght;
   uint8_t bNumInterfaces;
   uint8_t bConfigurationValue;
   uint8_t iConfiguration;
   uint8_t bmAttributes;
   uint8_t bMaxPower;
}__attribute__((packed))UsbConfigurationDescriptor;

typedef struct{
   uint8_t bLength;
   uint8_t bDescriptorType;
   uint8_t bInterfaceNumber;
   uint8_t bAlternateSetting;
   uint8_t bNumEndpoints;
   uint8_t bInterfaceClass;
   uint8_t bInterfaceSubClass;
   uint8_t bInterfaceProtocol;
   uint8_t iInterface;
}__attribute__((packed))UsbInterfaceDescriptor;

typedef struct{
   uint8_t bLength;
   uint8_t bDescriptorType;
   uint8_t bMaxBurst;
   union{
      uint8_t bmAttributes;
      struct{
         uint8_t maxStreams : 5;
         uint8_t reserved : 3;
      };
   };
   uint16_t wBytesPerInterval;
}__attribute__((packed))UsbSuperSpeedEndpointDescriptor;

typedef struct{
   uint8_t bLength;
   uint8_t bDescriptorType;
   union{
      uint8_t bEndpointAddress;
      struct{
         uint8_t endpointNumber : 4;
         uint8_t reserved : 3;
         uint8_t direction : 1;
      };
   };
   union{
      uint8_t bmAttributes;
      struct{
         uint8_t transferType : 2;
         uint8_t synchronizationType : 2;
         uint8_t usageType : 2;
         uint8_t reserved2 : 2;
      };
   };
   uint16_t wMaxPacketSize;
   uint8_t bInterval;
   UsbSuperSpeedEndpointDescriptor *superSpeedDescriptor;
}__attribute__((packed))UsbEndpointDescriptor;

typedef struct{
   UsbInterfaceDescriptor descriptor;
   UsbEndpointDescriptor *endpoints;
}UsbInterface;

typedef struct{
   UsbConfigurationDescriptor descriptor;
   UsbInterface *interfaces;
}UsbConfiguration;



#endif
