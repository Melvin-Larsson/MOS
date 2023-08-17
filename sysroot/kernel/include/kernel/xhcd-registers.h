#ifndef XHCD_REGISTERS_H_INCLUDED
#define XHCD_REGISTERS_H_INCLUDED

#include "stdint.h"

typedef volatile struct{
   uint32_t enabledDeviceSlots : 8;
   uint32_t U3EntryEnable : 1;
   uint32_t configureinformationenable : 1;
   uint32_t RsvdP : 22;
}__attribute__((packed))XhcConfigRegister;

typedef volatile struct{
   uint8_t maxDeviceSlots;
   uint16_t maxInterrupters : 11;
   uint8_t reserved : 5;
   uint8_t maxPorts;
}__attribute__((packed))StructParams1;

typedef volatile struct{
   uint32_t ist : 4;
   uint32_t erstMax: 4;
   uint32_t reserved : 13;
   uint32_t maxScratchpadBuffersHigh : 5;
   uint32_t scratchpadRestore : 1;
   uint32_t maxScratchpadBuffersLow : 5;
}__attribute__((packed))StructParams2;

typedef volatile struct{
   uint16_t other;
   uint16_t extendedCapabilitiesPointer;
}__attribute__((packed))CapabilityParams1;

typedef volatile struct{
   uint32_t currentConnectStatus : 1;
   uint32_t portEnabledDisabled : 1;
   uint32_t reserved : 1;
   uint32_t overCurrentActive : 1;
   uint32_t portReset : 1;
   uint32_t portLinkState : 4;
   uint32_t portPower : 1;
   uint32_t portSpeed : 4;
   uint32_t portIndicatorControl : 2;
   uint32_t portLinkStateWriteStrobe : 1;
   uint32_t connectStatusChange : 1;
   uint32_t portEnableDisableChange : 1;
   uint32_t warmPortResetChange : 1;
   uint32_t overCurrentChange : 1;
   uint32_t portResetChange : 1;
   uint32_t other : 10;
}__attribute__((packed))PortStatusAndControll;

typedef volatile struct{
   PortStatusAndControll statusAndControll;
   uint32_t powerStatusAndControll;
   uint32_t portLinkInfo;
   uint32_t portHardwareLPMControll;
}__attribute__((packed)) XhciPortRegisters;

typedef volatile struct{
   uint8_t capabilityId;
   uint8_t nextExtendedCapabilityPointer;
   uint16_t r0High;
   uint32_t body[1];

}__attribute__((packed))XhciExtendedCapabilities;

typedef volatile struct{
   uint32_t capabilityRegistersLength : 8;
   uint32_t reserved : 8;
   uint32_t interfaceVersionNumber : 16;
   StructParams1 structParams1;
   StructParams2 structParams2;
   uint32_t structParams3;
   CapabilityParams1 capabilityParams1;
   uint32_t doorbellOffset;
   uint32_t runtimeRegisterSpaceOffset;
   uint32_t capabilityParams2;
}__attribute__((packed))XhciCapabilities;

typedef volatile struct{
   uint32_t USBCommand;
   uint32_t USBStatus;
   uint32_t pageSize;
   uint64_t reserved;
   uint32_t deviceNotificationControll;
   uint64_t commandRingControll;
   
   uint64_t reserved2;
   uint64_t reserved3;
   uint64_t dcAddressArrayPointer;
   XhcConfigRegister configure;
   uint8_t reserved4[964];
   volatile XhciPortRegisters ports[256];
}__attribute__((packed))XhciOperation;

typedef volatile struct{
   uint32_t target : 8;
   uint32_t reserved : 8;
   uint32_t taskId : 16;
}__attribute__((packed))XhciDoorbell;


typedef volatile struct{
   uint32_t interruptPending : 1;
   uint32_t interruptEnable : 1;
   uint32_t reserved : 30;
   uint32_t moderationInterval : 16;
   uint32_t moderationCounter : 16;
   uint32_t eventRingSegmentTableSize;
   uint32_t reserved3;
   uint64_t eventRingSegmentTableAddress;
   uint64_t eventRingDequePointer;
}__attribute__((packed))InterrupterRegisters;


typedef volatile struct{
   union{
      struct{
         uint32_t routeString : 20;
         uint32_t speed : 4;
         uint32_t reserved : 1;
         uint32_t multiTT : 1;
         uint32_t hub : 1;
         uint32_t contextEntries : 5;
         uint32_t maxExitLatency : 16;
         uint32_t rootHubPortNumber : 8;
         uint32_t numberOfPorts : 8;
         uint32_t other[6];
      };
      uint32_t registers[8];
   };
}__attribute__((packed))XhcSlotContext;

typedef volatile struct{
   uint32_t endpointState : 3;
   uint32_t reserved : 5;
   uint32_t mult : 2;
   uint32_t maxPrimaryStreams : 5;
   uint32_t linearStreamArray : 1;
   uint32_t interval : 8;
   uint32_t maxESITPayloadHigh : 8;
   uint32_t reserved2 : 1;
   uint32_t errorCount : 2;
   uint32_t endpointType : 3;
   uint32_t reserved3 : 1;
   uint32_t hostInitiateDisable : 1;
   uint32_t maxBurstSize : 8;
   uint32_t maxPacketSize : 16;
   uint64_t dequeuePointer;
   uint32_t avarageTrbLength : 16;
   uint32_t maxESITPayloadLow : 16;
   uint32_t reserved4[3];
}__attribute__((packed))XhcEndpointContext;

typedef volatile struct{
   uint32_t dropContextFlags;
   uint32_t addContextFlags;
   uint32_t reserved[5];
   uint32_t configurationValue : 8;
   uint32_t interfaceNumber : 8;
   uint32_t alternateSettings : 8;
   uint32_t reserved2 : 8;
}__attribute__((packed))XhcInputControlContext;

typedef volatile struct{
   XhcInputControlContext inputControlContext;
   XhcSlotContext slotContext;
   XhcEndpointContext endpointContext[30];
}__attribute__((packed))XhcInputContext;

typedef volatile struct{
   XhcSlotContext slotContext;
   XhcEndpointContext endpointContext[30];
}__attribute__((packed))XhcOutputContext;


#endif
