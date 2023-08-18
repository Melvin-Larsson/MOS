#ifndef KEYBOARD_H_INCLUDE
#define KEYBOARD_H_INCLUDE

#include "xhcd.h"

typedef enum{
   KeyboardSuccess = 0,
   KeyboardInvalidConfiguration,
   KeyboardConfigureError,
   KeyboardProtocolError
}KeyboardStatus;

KeyboardStatus keyboard_init(Xhci *xhci, UsbDevice *device);
void keyboard_getStatusCode(KeyboardStatus status, char output[100]);


#endif
