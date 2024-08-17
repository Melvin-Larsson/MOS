#include "kernel/pit.h"
#include "kernel/ioport.h"
#include "stdlib.h"
#include "stdint.h"
#include "stdbool.h"

#define CHANNEL_0_DATA_PORT 0x40
#define CHANNEL_1_DATA_PORT 0x41
#define CHANNEL_2_DATA_PORT 0x42
#define MODE_COMMAND_REGISTER 0x43

#define SELECT_CHANNEL_POS 6
#define SELECT_CHANNEL_MASK (0b11000000)
#define ACCESS_MODE_POS 4
#define ACCESS_MODE_MASK (0b110000)
#define OPERATING_MODE_POS 1
#define OPERATING_MODE_MASK (0b1110)
#define BCD_BINARY_MODE_POS 0
#define BCD_BINARY_MODE_MASK 1

typedef enum{
   Channel0 = 0,
   Channel1 = 1,
   Channel2 = 2
}Channel;

typedef enum{
   LatchCountValue = 0,
   LowByteOnly = 1,
   HighByteOnly = 2,
   LowThenHighByte = 3,
}AccessMode;

typedef enum{
   InterruptOnTerminalCount = 0,
   HardwareReTriggerableOneShot = 1,
   RateGenerator = 2,
   SquareWaveGenerator = 3,
   SoftwareTriggeredStrobe = 4,
   HardwareTriggeredStrobe = 5,
}OperatingMode;

typedef struct{
   OperatingMode mode;
   uint16_t initialValue;
   bool useBcd;
}ChannelConfig;

static ChannelConfig channels[3];

static void writeCommand(Channel channel, AccessMode accessMode, OperatingMode operatingMode, bool useBcd);

static uint16_t readChannel(Channel channel);
static void writeChannel(Channel channel, uint16_t value);

void pit_init(){
   memset(channels, 0, sizeof(channels));
}

void pit_setTimer(void (*handler)(void *data), void *data, uint32_t time){
   writeChannel(Channel0, ~0);
   writeCommand(Channel0, LowThenHighByte, InterruptOnTerminalCount, false);
}

static uint16_t readChannel(Channel channel){
   writeCommand(channel, LowThenHighByte, channels[channel].mode, channels[channel].useBcd);
   uint8_t lowByte = ioport_in8(CHANNEL_0_DATA_PORT + channel);
   uint8_t highByte = ioport_in8(CHANNEL_0_DATA_PORT + channel);
   return highByte << 8 | lowByte;
}

static void writeChannel(Channel channel, uint16_t value){
   channels[channel].initialValue = value;
   writeCommand(channel, LowThenHighByte, channels[channel].mode, channels[channel].useBcd);

   ioport_out8(CHANNEL_0_DATA_PORT + channel, value & 0xFF);
   ioport_out8(CHANNEL_0_DATA_PORT + channel, value >> 8);
}

static void writeCommand(Channel channel, AccessMode accessMode, OperatingMode operatingMode, bool useBcd){
   ioport_out8(
         MODE_COMMAND_REGISTER,
         channel << SELECT_CHANNEL_POS | accessMode << ACCESS_MODE_POS | operatingMode << OPERATING_MODE_POS | useBcd << BCD_BINARY_MODE_POS);
}
