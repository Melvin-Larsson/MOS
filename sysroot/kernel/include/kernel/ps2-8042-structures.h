#ifndef PS2_8042_STRUCTURES_H_INCLUDED
#define PS2_8042_STRUCTURES_H_INCLUDED

#include "stdint.h"

#define OUTPUT_BUFFER_FULL (1 << 0)
#define INPUT_BUFFER_FULL (1 << 1)

#define CONTROLLER_TEST_PASSED 0x55
#define PORT_TEST_PASSED 0

#define DEVICE_COMMAND_RESET 0xFF
#define DEVICE_COMMAND_DISABLE_SCANNING 0xF5
#define DEVICE_COMMAND_IDENTIFY 0xF2
#define DEVICE_RESPONSE_ACK 0xFA
#define DEVICE_RESPONSE_TEST_PASSED 0xAA

#define PORT_1_IRQ 1
#define PORT_2_IRQ 12

#define DATA_PORT 0x60
#define STATUS_PORT 0x64
#define COMMAND_PORT 0x64

typedef union{
   uint8_t status;
   struct{
        uint8_t outputBufferFull : 1; 
        uint8_t inputBufferFull : 1; 
        uint8_t systemFlag : 1;
        uint8_t target : 1;
        uint8_t unknown : 2;
        uint8_t timeOutError : 1;
        uint8_t parityError : 1;
   };
}Status;

typedef enum {
    readControllerConfigByte = 0x20,
    disablePort2 = 0xA7,
    enablePort2 = 0xA8,
    testPort2 = 0xA9,
    testController = 0xAA,
    testPort1 = 0xAB,
    diagnosticDump = 0xAC,
    disablePort1 = 0xAD,
    enablePort1 = 0xAE,
    readControllerInputPort = 0xC0,
    copyInputBits0to3ToStatus4to7 = 0xC1,
    copyInputBits4to7ToStatus4to7 = 0xC2,
    readControllerOutputPort = 0xD0,
}Command;

typedef enum {
    writeControllerConfigByte = 0x60,
    writeControllerOutputPort = 0xD1,
    writePort1OutputBuffer = 0xD2,
    writePort2OutputBuffer = 0xD3,
    writePort2InputBuffer = 0xD4,
}ArgumentCommand ;

typedef enum{
    readInternalRamN = 0x21, // 0x21 to 0x3F (N = command byte & 0x1F)
    writeInternalRamN = 0x61, // 0x61 to 0x7F (N = command byte & 0x1F)
    pulseOutputLineLow = 0xF0 // 0xF0 to 0xFF
}MaskCommand;

typedef union {
   struct{
      uint8_t enableInterruptPort1 : 1;
      uint8_t enableInterruptPort2 : 1;
      uint8_t systemFlag : 1;
      uint8_t reserved1 : 1; // Should be zero
      uint8_t disableClockPort1 : 1;
      uint8_t disableClockPort2 : 1;
      uint8_t enablePort1Translation : 1;
      uint8_t reserved2 : 1; // Must be zero
   };
   uint8_t config;
} Ps2ControllerConfig;

typedef struct {
    uint8_t systemReset : 1;
    uint8_t a20Gate : 1;
    uint8_t port2Clock : 1;
    uint8_t port2Data : 1;
    uint8_t port1OutputBufferFull : 1;
    uint8_t port2PortOutputBufferFull : 1;
    uint8_t port1Clock : 1;
    uint8_t port1Data : 1;
} Ps2ControllerOutput;

#endif
