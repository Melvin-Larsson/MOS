#ifndef PS2_8042_H_INCLUDED
#define PS2_8042_H_INCLUDED

#include "stdint.h"
#include "stdbool.h"

typedef enum{
   Ps28042StatusSucess,
   Ps28042TimedOut,
   Ps28042ControllerTestFailed,
   Ps28042NoWorkingPorts,
   Ps28042NothingToRead
}Ps28042Status;

typedef enum{
   Port1 = 1,
   Port2 = 2
}Ps28042PortId;

typedef struct{
   Ps28042PortId portId;
   bool functional;
   bool attached;
   uint16_t deviceType;
   bool missingDeviceType;
}Ps28042Port;

Ps28042Status ps2_8042_init();

Ps28042Port ps2_8042_getPortInfo(Ps28042PortId portId);

Ps28042Status ps2_8042_writeToPort(Ps28042PortId portId, uint8_t data);
Ps28042Status ps2_8042_readPort(Ps28042PortId portId, uint8_t *result);
Ps28042Status ps2_8042_readPortBlocking(Ps28042PortId portId, uint8_t *result);

void ps2_8042_set_interrupt_handler(Ps28042PortId portId, void (*handler)(uint8_t));



#endif
