#ifndef KEYBOARD_H_INCLUDE
#define KEYBOARD_H_INCLUDE

#include "kernel/usb.h"

typedef enum{
   KeyboardSuccess = 0,
   KeyboardInvalidConfiguration,
   KeyboardConfigureError,
   KeyboardProtocolError
}KeyboardStatus;

KeyboardStatus keyboard_init(UsbDevice2 *usbDevice);
void keyboard_getStatusCode(KeyboardStatus status, char output[100]);


#endif
