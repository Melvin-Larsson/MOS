#include "kernel/ps2-8042.h"
#include "kernel/ps2-8042-structures.h"
#include "kernel/ioport.h"
#include "kernel/logging.h"
#include "kernel/interrupt.h"
#include "kernel/ioapic.h"
#include "kernel/acpi.h"
#include "kernel/paging.h"
#include "stdlib.h"
#include "stddef.h"
#include "utils/assert.h"

#define readStatus() ioport_in8(0x64)

#define RETRY_COUNT 20
#define RETRY_DELAY_MS 10

#define BUFFER_SIZE 128

#define APIC_EOI_ADDRESS 0xFEE000B0 //FIXME: move

typedef struct{
   Ps28042Port portInfo;
   volatile uint8_t buffer[BUFFER_SIZE];
   volatile size_t enque;
   volatile size_t deque;
   void (*interrupt_handler)(uint8_t);
}Port;

static bool initPort(Ps28042PortId portId, Port *result);
static bool identifyPort(Ps28042PortId portId, uint8_t *result, uint8_t *bytes_sent);
static bool resetPort(Ps28042PortId portId);
static bool verifyReset();
static bool testChannel(Ps28042PortId port, bool *success);

static bool enablePort(Ps28042PortId portId);
static void enableInterrupts(Port *port);
static void flushOutputBuffer();

static bool writeToPort(Ps28042PortId portId, uint8_t data);

static bool writeArgumentlessCommand(Command command);
static bool writeArgumentCommand(ArgumentCommand command, uint8_t argument);

static bool readData(uint8_t *data);
static bool waitStatus(uint8_t set, uint8_t clear);
static bool writeCommand(uint8_t command);
static bool writeData(uint8_t command);

static Port port1;
static Port port2;

Ps28042Status ps2_8042_init(){
   loggDebug("Init");
   //FIXME: Should look at acpi to verify this port exists

   writeArgumentlessCommand(disablePort1);
   writeArgumentlessCommand(disablePort2);

   flushOutputBuffer();

   Ps2ControllerConfig config;
   writeArgumentlessCommand(readControllerConfigByte);
   if(!readData(&config.config)){
      return Ps28042TimedOut;
   }
   config.disableClockPort1 = 0;
   config.disableClockPort2 = 1;
   config.enableInterruptPort1 = 0;
   config.enableInterruptPort2 = 0;
   config.enablePort1Translation = 0;
   writeArgumentCommand(writeControllerConfigByte, config.config);
   loggDebug("Configured");

   writeArgumentlessCommand(testController);
   uint8_t controllerTestResult;
   if(!readData(&controllerTestResult)){
      return Ps28042TimedOut;
   }
   if(controllerTestResult != CONTROLLER_TEST_PASSED){
      return Ps28042ControllerTestFailed;
   }
   loggDebug("Controller test passed");
   writeArgumentCommand(writeControllerConfigByte, config.config); //The test might reset the device

   initPort(Port1, &port1);
   initPort(Port2, &port2);

   if(!port1.portInfo.functional && !port2.portInfo.functional){
      return Ps28042NoWorkingPorts;
   }

   if(port1.portInfo.attached){
       enableInterrupts(&port1);
   }
   if(port2.portInfo.attached){
       enableInterrupts(&port2);
   }

   return Ps28042StatusSucess;
}

Ps28042Status ps2_8042_writeToPort(Ps28042PortId portId, uint8_t data){
   if(writeToPort(portId, data)){
      return Ps28042StatusSucess;
   }
   return Ps28042TimedOut;
}

Ps28042Status ps2_8042_readPort(Ps28042PortId portId, uint8_t *result){
   Port *port = portId == Port1 ? &port1 : &port2;

   if(port->deque == port->enque){
      return Ps28042NothingToRead;
   }

   *result = port->buffer[port->deque];
   port->deque = (port->deque + 1) % sizeof(port->buffer);

   return Ps28042StatusSucess;
}

Ps28042Status ps2_8042_readPortBlocking(Ps28042PortId portId, uint8_t *result){
    loggDebug("Read port blocking");
    while(1){
        Ps28042Status status = ps2_8042_readPort(portId, result);
        if(status != Ps28042NothingToRead){
            return status;
        }
    }
}

Ps28042Port ps2_8042_getPortInfo(Ps28042PortId portId){
   return portId == Port1 ? port1.portInfo : port2.portInfo;
}

void ps2_8042_set_interrupt_handler(Ps28042PortId portId, void (*handler)(uint8_t)){
   Port *port = portId == Port1 ? &port1 : &port2;
   port->interrupt_handler = handler;
}

static void port_handler(void *portData){
   Port *port = (Port*)portData;

   uint8_t data;
   if(readData(&data)){
      loggDebug("Data %X read (port %d)", data, port->portInfo.portId);
      if(port->interrupt_handler != 0){
          loggDebug("Invoking handler");
          port->interrupt_handler(data);
      }
      else{
         size_t next = (port->enque + 1) % sizeof(port->buffer);
         if(next == port->deque){
            loggWarning("Buffer full");
         }
         else{
            port->buffer[port->enque] = data;
            port->enque = next;
         }
      }
   }

   uint32_t eoiData = 0;
   paging_writePhysicalOfSize(APIC_EOI_ADDRESS, &eoiData, 4, AccessSize32);
}

static void enableInterrupts(Port *port){
     uint8_t vector = interrupt_setHandler(port_handler, port, "ps2-8042");
     loggDebug("PS/2 port %d has interrupt vector %d", port->portInfo.portId, vector);
     LocalApicData localApic;
     assert(acpi_getLocalApicData(&localApic));

     IRQConfig irqConfig = ioapic_getDefaultIRQConfig(localApic.apicId, vector);
     uint8_t irqNumber = port->portInfo.portId == Port1 ? PORT_1_IRQ : PORT_2_IRQ;
     ioapic_configureIrq(irqNumber, irqConfig);

     writeArgumentlessCommand(readControllerConfigByte);
     Ps2ControllerConfig config;
     readData(&config.config);
     if(port->portInfo.portId == Port1){
        config.enableInterruptPort1 = 1;
     }else{
        config.enableInterruptPort2 = 1;
     }

     writeArgumentCommand(writeControllerConfigByte, config.config);
}

static bool initPort(Ps28042PortId portId, Port *result){
   memset(result, 0, sizeof(Port));
   result->portInfo.portId = portId;

   if(!testChannel(portId, &result->portInfo.functional)){
       loggDebug("Port %d not functional", portId);
       return false;
   }

   if(result->portInfo.functional){
      if(!enablePort(portId)){
         return false;
      }
      loggDebug("Port %d enabled", portId);

      result->portInfo.attached = resetPort(portId);
      if(!result->portInfo.attached){
         return true;
      }

      uint8_t type[2];
      uint8_t result_length;
      if(!identifyPort(portId, type, &result_length)){
         result->portInfo.functional = false;
         result->portInfo.attached = false;
         loggError("Unable to identify port %d", portId);
      }
      else if(result_length == 0){
         result->portInfo.missingDeviceType = true;
      }
      else{
         result->portInfo.missingDeviceType = false;
         memcpy(&result->portInfo.deviceType, type, result_length);
         loggInfo("Device of type %X detected on port %d", result->portInfo.deviceType, portId);
      }
   }

   return true;
}

static bool identifyPort(Ps28042PortId portId,uint8_t *result, uint8_t *bytes_sent){
   writeToPort(portId, DEVICE_COMMAND_DISABLE_SCANNING);
   uint8_t ack1;
   if(!readData(&ack1) || ack1 != DEVICE_RESPONSE_ACK){
      loggError("Port %d: Unable to disable scanning", portId);
      return false;
   }

   writeToPort(portId, DEVICE_COMMAND_IDENTIFY);
   uint8_t ack2;
   if(!readData(&ack2) || ack2 != DEVICE_RESPONSE_ACK){
      loggError("Port %d: Unable to identify", portId);
      return false;
   }
   
   for(*bytes_sent = 0; *bytes_sent < 2; (*bytes_sent)++){
      if(!readData(&result[*bytes_sent])){
         return true; 
      }
   }

   return true;
}


static bool resetPort(Ps28042PortId portId){
   if(!writeToPort(portId, DEVICE_COMMAND_RESET)){
      return false;
   }
   return verifyReset();
}

static bool verifyReset(){
   uint8_t buffer[3];
   if(!readData(&buffer[0]) || !readData(&buffer[1])){
      return false;
   }
   readData(&buffer[2]);

   if(!(buffer[0] == DEVICE_RESPONSE_ACK && buffer[1] == DEVICE_RESPONSE_TEST_PASSED) 
     && !(buffer[1] == DEVICE_RESPONSE_TEST_PASSED && buffer[0] == DEVICE_RESPONSE_ACK)){
      return false;
   }

   return true;
}
static bool testChannel(Ps28042PortId portId, bool *success){
   if(portId == Port1){
      uint8_t testResult;
      if(!writeArgumentlessCommand(testPort1) || !readData(&testResult)){
         loggDebug("Timed out channel 1\n");
         return false;
      }

      if(testResult == PORT_TEST_PASSED){
         loggInfo("Port 1 is functional");
      }
      else{
         loggError("Port 1 is non functional. Reason %d", testResult);
      }
      *success = testResult == PORT_TEST_PASSED;
      
      return true;
   }
   else{
      Ps2ControllerConfig config;
      if(!writeArgumentlessCommand(enablePort2) ||
         !writeArgumentlessCommand(readControllerConfigByte) ||
         !readData(&config.config)){

          loggError("Fuck");
         return false;
      }

      if(config.disableClockPort2){
         *success = false;
         return true;
      }

      //This seems a bit strange, but https://wiki.osdev.org/%228042%22_PS/2_Controller
      //says I should do this
      if(!writeArgumentlessCommand(disablePort2)){
         return false;
      }
      config.disableClockPort2 = 0;
      config.enableInterruptPort2 = 0;
      if(!writeArgumentCommand(writeControllerConfigByte, config.config)){
         return false;
      }

      uint8_t testResult;
      if(!writeArgumentlessCommand(testPort2) || !readData(&testResult)){
         return false;
      }

      if(testResult == PORT_TEST_PASSED){
         loggInfo("Port 2 is functional");
      }
      else{
         loggError("Port 2 is non functional. Reason %d", testResult);
      }
      *success = testResult == PORT_TEST_PASSED;

      return true;
   }
}

static bool enablePort(Ps28042PortId portId){
   return writeArgumentlessCommand(portId == Port1 ? enablePort1 : enablePort2);
}

static bool writeToPort(Ps28042PortId portId, uint8_t data){
   if(portId == Port1){
      return writeData(data);
   }

   return writeArgumentCommand(writePort2InputBuffer, data);
}

static void flushOutputBuffer(){
   ioport_in8(STATUS_PORT);
}

static bool writeArgumentlessCommand(Command command){
   return writeCommand(command);
}

static bool writeArgumentCommand(ArgumentCommand command, uint8_t argument){
   return writeCommand(command) && writeData(argument);
}

static bool writeCommand(uint8_t command){
   if(!waitStatus(0, INPUT_BUFFER_FULL)){
      return false;
   }

   ioport_out8(COMMAND_PORT, command);
   return true;
}

static bool writeData(uint8_t command){
   if(!waitStatus(0, INPUT_BUFFER_FULL)){
      return false;
   }

   ioport_out8(DATA_PORT, command);
   return true;
}

static bool readData(uint8_t *data){ 
   if(!waitStatus(OUTPUT_BUFFER_FULL, 0)){
      return false;
   }

   *data = ioport_in8(DATA_PORT);
   return true;
} 

static bool waitStatus(uint8_t set, uint8_t clear){
    //FIXME: Use real delay here
   for(uint32_t i = 0; i < RETRY_COUNT * 10000; i++){
      uint8_t status = readStatus();
      if((status & clear) == 0 && (status & set) == set){
         return true;
      }
//       thread_sleep(RETRY_DELAY_MS);
   }
   return false;
}
