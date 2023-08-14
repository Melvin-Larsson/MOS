#ifndef USB_DESCRIPTORS_H_INCLUDED
#define USB_DESCRIPTORS_H_INCLUDED

#include "stdint.h"

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
   uint8_t bEndpointAddress;
   uint8_t bmAttributes;
   uint16_t wMaxPacketSize;
   uint8_t bInterval;
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
