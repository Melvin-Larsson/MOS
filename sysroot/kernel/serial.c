#include "kernel/serial.h"

#define DIVISOR_LATCH_ACCESS_BIT (1 << 7)
#define DIVISOR_REGISTER_LSB 0
#define DIVISOR_REGISTER_MSB 1

#define DATA_BITS_POS 0
#define STOP_BIT_POS 2
#define PARITY_BITS_POS 3
#define BREAK_ENABLE_BIT_POS 6
#define DIVISOR_LATCH_ACCESS_BIT_POS 7

#define RECEIVE_DATA_AVAILABLE (1 << 0)
#define INTERRUPT_ON_TRANSMISSION_BUFFER_EMPTY (1 << 1)
#define RECEIVER_LINE_STATUS (1 << 2)
#define MODEM_STATUS (1 << 3)

#define ENABLE_FIFO (1 << 0)
#define CLEAR_RECEIVE_FIFO (1 << 1)
#define CLEAR_TRANSMIT_FIFO (1 << 2)
#define DMA_MODE_SELECT (1 << 3)
#define INTERRUPT_TRIGGER_LEVEL_POS 6

#define INTERRUPT_PENDING (1 << 0)
#define INTERRUPT_STATE_POS 1
#define INTERRUPT_STATE_MASK 0b110
#define TIMEOUT_INTERRUPT_PENDING (1 << 3)
#define FIFO_BUFFER_STATE_POS 6
#define FIFO_BUFFER_STATE_MASK 0b11000000

#define TRANSMISSION_BUFFER_EMPTY (1 << 5)
#define DATA_READY (1 << 0)

#define DTR (1 << 0)
#define RTS (1 << 1)

#define DATA (1 << 5)

#define ASSERTS_ENABLED
#include "utils/assert.h"

typedef enum{
   ReceiveBuffer = 0,
   TransmitBuffer = 0,
   InterruptEnable = 1,

   InterruptIdentification = 2,
   FifoControl = 2,
   LineControlRegister = 3,
   ModemControlRegister = 4,
   LineStatusRegister = 5,
   ModemStatusRegister = 6,
   ScratchRegister = 7,
}SerialRegister;

typedef struct{
   uint32_t baseAddress;
}SerialPortData;

typedef enum{
   ModemStatus = 0,
   TransmitterHoldingRegisterEmpty = 1,
   ReceviedDataAvailable = 2,
   ReceiverLineStatus = 3
}InterruptState;

typedef enum{
   NoFifo = 0,
   FifoUnusable = 1,
   FifoEnabled = 2
}FifoBufferState;

static SerialPortData ports[8];

static uint32_t getBaseAddress(SerialPortNumber port);
static void doHandshake(SerialPortData port);
static void initInterrupts(SerialPortData port, InterruptTriggerLevel interruptTriggerLevel);
static void configureLineControl(SerialPortData port, SerialPortConfig config);
static void setDivisorValue(SerialPortData port, uint16_t value);

static void orRegister(SerialPortData port, SerialRegister serialRegister, uint8_t orValue);
static void andRegister(SerialPortData port, SerialRegister serialRegister, uint8_t andValue);
static void andOrRegister(SerialPortData port, SerialRegister serialRegister, uint8_t andValue, uint8_t orValue);
static void writeRegister(SerialPortData port, SerialRegister serialRegister, uint8_t value);
static uint8_t readRegister(SerialPortData port, SerialRegister serialRegister);

SerialPortConfig serial_defaultConfig(){
   return (SerialPortConfig){
      .rateDivisor = 1,
      .charLength = CharLength8,
      .stopBitCount = StopBitCount1,
      .parityBit = ParityBitNone,
      .interruptTriggerLevel = TriggerLevel8Bytes
   };
} 

SerialStatus serial_initPort(SerialPortNumber port, SerialPortConfig config){
   if(config.rateDivisor == 0){
      return SerialInvalidConfig;
   }

   ports[port] = (SerialPortData){
      .baseAddress = getBaseAddress(port)
   };

   setDivisorValue(ports[port], config.rateDivisor);
   configureLineControl(ports[port], config);
   initInterrupts(ports[port], config.interruptTriggerLevel);
   doHandshake(ports[port]);

   return SerialOk;
}

SerialStatus serial_write(SerialPortNumber port, const char* data){
   while(*data){
      while(!(readRegister(ports[port], LineStatusRegister) & TRANSMISSION_BUFFER_EMPTY));
      for(int i = 0; i < 16 && *data; i++){
         writeRegister(ports[port], TransmitBuffer, *data);
         data++;
      }
   }
/*    if(enque == deque){ */
/*       for(int i = 0; i < 16 && *data; i++){ */
/*          writeRegister(ports[port], TransmitBuffer, *data); */
/*          data++; */
/*       } */
/*    } */
/*    while(*data){ */
/*       buffer[enque] = *data; */
/*       enque++; */
/*       data++; */
/*    } */
   return SerialOk;
}

/* static void handler(void *data){ */
/*    SerialPortData serialPortData = *(SerialPortData*)data; */
/*    uint8_t status = readRegister(serialPortData, LineStatusRegister); */
/*    if(status & TRANSMISSION_BUFFER_EMPTY){ */
/*       for(int i = 0; i < 16 && deque < enque; i++){ */
/*          writeRegister(serialPortData, TransmitBuffer, buffer[deque]); */
/*          deque++; */
/*       } */
/*    } */
/* } */

uint32_t serial_read(SerialPortNumber port, char *result, uint32_t bufferSize){
   uint32_t count = 0;
   while(bufferSize && readRegister(ports[port], LineStatusRegister) & DATA_READY){
      *result = readRegister(ports[port], ReceiveBuffer);
      result++;
      count++;
   }
   return count;
}

static uint32_t getBaseAddress(SerialPortNumber port){
   //FIXME:
   switch(port){
      case COM1: 
         return 0x3F8;
      case COM2:
         return 0x2F8;
      default:
         return 0;
   }
}

static void doHandshake(SerialPortData port){
   uint8_t modemControlValue = DTR | RTS | (1 << 3);
   writeRegister(port, ModemControlRegister, modemControlValue);
}

static void initInterrupts(SerialPortData port, InterruptTriggerLevel interruptTriggerLevel){
   uint8_t interruptEnableValue = RECEIVE_DATA_AVAILABLE | INTERRUPT_ON_TRANSMISSION_BUFFER_EMPTY;
   writeRegister(port, InterruptEnable, 0);

   uint8_t fifoControl = ENABLE_FIFO | CLEAR_RECEIVE_FIFO | CLEAR_TRANSMIT_FIFO | (interruptTriggerLevel << INTERRUPT_TRIGGER_LEVEL_POS);
   writeRegister(port, FifoControl, fifoControl);
}

static void configureLineControl(SerialPortData port, SerialPortConfig config){
   uint8_t lineControlValue = (config.parityBit << PARITY_BITS_POS)
      | (config.stopBitCount << STOP_BIT_POS)
      | (config.charLength << DATA_BITS_POS);

   writeRegister(port, LineControlRegister, lineControlValue);
   uint8_t reg = readRegister(port, LineControlRegister);
}

static void setDivisorValue(SerialPortData port, uint16_t value){
   orRegister(port, LineControlRegister, DIVISOR_LATCH_ACCESS_BIT);
   writeRegister(port, DIVISOR_REGISTER_LSB, value & 0xFF);
   writeRegister(port, DIVISOR_REGISTER_MSB, value >> 8);
   andRegister(port, LineControlRegister, (uint8_t)~DIVISOR_LATCH_ACCESS_BIT);
}

static void andRegister(SerialPortData port, SerialRegister serialRegister, uint8_t andValue){
   andOrRegister(port, serialRegister, andValue, 0);
}

static void orRegister(SerialPortData port, SerialRegister serialRegister, uint8_t orValue){
   andOrRegister(port, serialRegister, 0xFF, orValue);
}

static void andOrRegister(SerialPortData port, SerialRegister serialRegister, uint8_t andValue, uint8_t orValue){
   uint8_t value = readRegister(port, serialRegister);
   value &= andValue;
   value |= orValue;
   writeRegister(port, serialRegister, value);
}

static void writeRegister(SerialPortData port, SerialRegister serialRegister, uint8_t value){
   __asm__ volatile("out %%al, %%dx"
      :
      : [data]"ax"(value), [address]"dx"(port.baseAddress + serialRegister)
      : 
      );
} 

static uint8_t readRegister(SerialPortData port, SerialRegister serialRegister){
   uint8_t result;
   __asm__ volatile("in %%dx, %%al"
      : [result]"=ax"(result)
      : [address]"dx"(port.baseAddress + serialRegister)
      : 
      );
   return result;
} 

