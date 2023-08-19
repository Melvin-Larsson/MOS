#ifndef USB_MESSAGES_H_INCLUDED
#define USB_MESSAGES_H_INCLUDED

#include "stdint.h"

typedef struct{
   uint8_t bmRequestType;
   uint8_t bRequest;
   uint16_t wValue;
   uint16_t wIndex;
   uint16_t wLength;
   void* dataBuffer;
}UsbRequestMessage;


#endif
