#ifndef SERIAL_H_INCLUDED
#define SERIAL_H_INCLUDED

#include "stdint.h"

typedef enum{
   SerialOk,
   SerialInvalidConfig,
}SerialStatus;

typedef enum{
   COM1 = 0,
   COM2,
   COM3,
   COM4,
   COM5,
   COM6,
   COM7,
   COM8,
}SerialPortNumber;

typedef enum{
   CharLength5 = 0,
   CharLength6 = 1,
   CharLength7 = 2,
   CharLength8 = 3,
}CharLength;

typedef enum{
   StopBitCount1 = 0,
   StopBitCount2 = 1
}StopBitCount;

typedef enum{
   ParityBitNone = 0,
   ParityBitOdd = 1,
   ParityBitEven = 3,
   ParityBitMark = 5,
   ParityBitSpace = 7
}ParityBit;

typedef enum{
   TriggerLevel1Byte = 0,
   TriggerLevel4Bytes = 1,
   TriggerLevel8Bytes = 2,
   TriggerLevel14Bytes = 3,
}InterruptTriggerLevel;

typedef struct{
   uint32_t rateDivisor;
   CharLength charLength;
   StopBitCount stopBitCount;
   ParityBit parityBit;
   InterruptTriggerLevel interruptTriggerLevel;

}SerialPortConfig;

SerialPortConfig serial_defaultConfig();
SerialStatus serial_initPort(SerialPortNumber port, SerialPortConfig config);
SerialStatus serial_write(SerialPortNumber port, char *data);
uint32_t serial_read(SerialPortNumber port, char *result, uint32_t bufferSize);

#endif
