#include "kernel/pit.h"
#include "kernel/ioport.h"
#include "kernel/logging.h"
#include "kernel/interrupt.h"
#include "kernel/acpi.h"
#include "kernel/ioapic.h"
#include "kernel/paging.h"
#include "stdlib.h"
#include "stdint.h"
#include "stdbool.h"

#include "utils/assert.h"

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

#define APIC_EOI_ADDRESS 0xFEE000B0 //FIXME: move

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

typedef struct{
   void (*handler)(void *, uint16_t);
   void *data;
}InterruptData;

static InterruptData interruptData;
static ChannelConfig channels[3];

static void writeCommand(Channel channel, AccessMode accessMode, OperatingMode operatingMode, bool useBcd);

static uint16_t readChannel(Channel channel);
static void writeChannel(Channel channel, uint16_t value);

static void handler(void *data);

static uint8_t pitInterruptVector;

void pit_init(){
   memset(channels, 0, sizeof(channels));
   memset(&interruptData, 0, sizeof(InterruptData));

   writeChannel(Channel0, 1);
   writeCommand(Channel0, LowThenHighByte, InterruptOnTerminalCount, false);

   while(readChannel(Channel0) <= 1);

   pitInterruptVector = interrupt_setHandler(handler, 0, "pit");
   loggDebug("Pit got interrupt vector %d", pitInterruptVector);
   if(pitInterruptVector == 0){
      return;
   }

   LocalApicData localApic;
   assert(acpi_getLocalApicData(&localApic));
   loggDebug("Local apic has id %d", localApic.apicId);

   IRQConfig irqConfig = ioapic_getDefaultIRQConfig(localApic.apicId, pitInterruptVector);
   ioapic_configureIrq(2, irqConfig); //Why 2?
}

void pit_setTimer(void (*handler)(void *, uint16_t), void *data, uint32_t pitCycles){
   if(!assert(pitCycles <= 0xFFFF)){
      return;
   }
   interruptData = (InterruptData){
      .handler = handler,
      .data = data,
   };

   writeChannel(Channel0, pitCycles);
   writeCommand(Channel0, LowThenHighByte, InterruptOnTerminalCount, false);
}

void pit_setDirectTimer(void (*handler)(void), uint32_t pitCycles){
   if(!assert(pitCycles <= 0xFFFF)){
      return;
   }

   interrupt_setHardwareHandler(handler, pitInterruptVector, Ring3);
   writeChannel(Channel0, pitCycles);
   writeCommand(Channel0, LowThenHighByte, InterruptOnTerminalCount, false);
}

uint16_t pit_getCycles(){
   writeCommand(Channel0, 0, 0, 0);
   uint16_t currCount = readChannel(Channel0);
   return channels[Channel0].initialValue - currCount;
}

void pit_stopTimer(){
   //FIXME
}

void pit_checkoutInterrupt(){
   uint32_t eoiData = 0;
   paging_writePhysicalOfSize(APIC_EOI_ADDRESS, &eoiData, 4, AccessSize32);
}

uint64_t pit_cyclesToNanos(uint64_t cycles){
   assert(cycles * 1000000000 >= cycles); //Overflow?
   return cycles * 1000000000 / 1193182;
}

uint64_t pit_nanosToCycles(uint64_t nanos){
   assert(nanos * 1193182 >= nanos); //Overflow?
   return nanos * 1193182 / 1000000000;
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

static void handler(void *data){
   (void)data;
   uint32_t eoiData = 0;
   paging_writePhysicalOfSize(APIC_EOI_ADDRESS, &eoiData, 4, AccessSize32);

   if(interruptData.handler){
      interruptData.handler(interruptData.data, channels[Channel0].initialValue); //FIXME: not exact
   }
}
