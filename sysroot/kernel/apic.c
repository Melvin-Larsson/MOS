#include "kernel/apic.h"
#include "kernel/paging.h"
#include "kernel/logging.h"
#include "stdio.h"
#include "stdint.h"

typedef struct{
   uint8_t reserved;
   uint8_t isBootstrapProcessor : 1;
   uint8_t reserved2 : 2;
   uint8_t enabled : 1;
   uint32_t apicBase : 24; //end address is indicated by cpuid.CPUID.80000008H:EAX[bits 7:0] if that leaf is supported. Otherwise 36
   uint32_t reserved3: 24; 
}IA32_APIC_BASE_MSR; //MSR address 1BH

typedef struct{
   uint8_t version;
   uint8_t reserved;
   uint8_t lvtEntries; //total -1
   uint8_t surpressEOIBroadcastSupported : 1;
   uint8_t reserved2 : 1;
}ApicVersionRegister;

#define APIC_DELIVERY_MODE_FIXED 000b
#define APIC_DELIVERY_MODE_SMI 010b
#define APIC_DELIVERY_MODE_NMI 100b
#define APIC_DELIVERY_MODE_INIT 101b
#define APIC_DELIVERY_MODE_EXT_INT 111b

//Only for Interrupt Comamand register
#define APIC_DELIVERY_MODE_LOWEST_PRIORITY 001b
#define APIC_DELIVERY_MODE_INIT_DE_ASSERT 101b
#define APIC_DELIVERY_MODE_STARTUP 110b

#define APIC_INPUT_PIN_POLARITY_ACTIVE_HIGH 0
#define APIC_INPUT_PIN_POLARITY_ACTIVE_LOW 1

#define APIC_TRIGGER_MODE_EDGE_SENSITIVE 0
#define APIC_TRIGGER_MODE_LEVEL_SENSITIVE 1

#define APIC_MASK_ENABLE 0
#define APIC_MASK_INHIBIT 1

#define APIC_TIMER_MODE_ONE_SHOT 00b
#define APIC_TIMER_PERIODIC 01b
#define APIC_TIMER_TSC_DEADLINE 10b
typedef struct{
   uint8_t interruptVectorNr;
   uint8_t deliveryMode : 3;
   uint8_t reserved : 1;
   const uint8_t deliveryStatus : 1;
   uint8_t inputPinPolarity : 1;
   const uint8_t remoteIrrFlag : 1;
   uint8_t triggerMode : 1;
   uint8_t mask : 1;
   uint8_t timerMode : 2;
   uint16_t reserved2 : 13;
}ApicLVTEntry;

typedef struct{
   uint8_t sendChecksumError : 1;
   uint8_t recieveCHecksumError : 1;
   uint8_t sendAcceptError : 1;
   uint8_t recieveAcceptError : 1;
   uint8_t redirectableIPI : 1;
   uint8_t sendIllegalVector : 1;
   uint8_t receiveIllegalVector : 1;
   uint8_t illegalRegisterAddress : 1;
   uint32_t reserved : 24;

}ApicErrorStatusRegister;

typedef struct{
   uint8_t value : 4;
   uint32_t reserved : 28;
} ApicTimerDivideConfRegister;

typedef struct uint32_t ApicTimerCount;

#define APIC_DESTINATION_MODE_PHYSICAL 0
#define APIC_DESTINATION_MODE_LOGICAL 0

#define APIC_DESTINATION_SHORTHAND_NO_SHORTHAND 00b
#define APIC_DESTINATION_SHORTHAND_SELF 01b
#define APIC_DESTINATION_SHORTHAND_ALL 10b
#define APIC_DESTINATION_SHORTHAND_ALL_EXC_SELF 11b

typedef struct{
   union{
      struct{
         uint8_t interruptVectorNr;
         uint8_t deliveryMode : 3;
         uint8_t destinationMode : 1;
         const uint8_t deliveryStatus : 1;
         uint8_t reserved : 1;
         uint8_t level : 1;
         uint8_t triggerMode : 1;
         uint8_t reserved2 : 2;
         uint8_t destinationShorthand : 2;
         uint64_t reserved3 : 36;
         uint8_t destination;
     };
      struct{
         uint32_t low;
         uint32_t high;
      };
   };
} ApicInterruptCommandRegister;

typedef struct{
   uint32_t reserved : 24;
   uint8_t logicalApicId;
}ApigcLogicalDestinationRegister;

#define APIC_DFR_MODEL_FLAT 1111b
#define APIC_DFR_MODEL_CLUSTER 0000b
typedef struct{
   uint32_t reserved : 29;
   uint8_t model : 4;
}ApicDestinationFormatRegister;

#define CPUID_APIC_BIT (1<<9)

#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_BSP 0x100 // Processor is a BSP
#define IA32_APIC_BASE_MSR_ENABLE 0x800

static void setApicBase(uintptr_t apic);
static uintptr_t getApicBase();

static void writeMsr(uint32_t eax, uint32_t edx, uint32_t msr);
static void readMsr(uint32_t msr, uint32_t *eax, uint32_t *edx);

int apic_isPresent(){
    uint32_t edx = 0;
   __asm__ volatile ("\
         mov $0x1, %%eax  \n\
         cpuid         \n\
         mov %%eax, %0" : "=r"(edx));
        
   return edx & CPUID_APIC_BIT ? 1 : 0;
}

void apic_enable(){
   return;
   uintptr_t apicBase = getApicBase();
   loggDebug("apic base %X\n", apicBase);
   setApicBase(apicBase);

   uint32_t reg;
   paging_readPhysical(apicBase + 0xF0, &reg, 4);
   reg |= 0x100;
   paging_writePhysical(apicBase + 0xF0, &reg, 4);
}

//Stolen from https://wiki.osdev.org/APIC
static void setApicBase(uintptr_t apic){
   uint32_t edx = 0;
   uint32_t eax = (apic & 0xfffff0000) | IA32_APIC_BASE_MSR_ENABLE;

#ifdef __PHYSICAL_MEMORY_EXTENSION__
   edx = (apic >> 32) & 0x0f;
#endif

   writeMsr(IA32_APIC_BASE_MSR, eax, edx);
}

//Stolen from https://wiki.osdev.org/APIC
static uintptr_t getApicBase() {
   uint32_t eax, edx;
   readMsr(IA32_APIC_BASE_MSR, &eax, &edx);

#ifdef __PHYSICAL_MEMORY_EXTENSION__
   return (eax & 0xfffff000) | ((edx & 0x0f) << 32);
#else
   return (eax & 0xfffff000);
#endif
}

static void writeMsr(uint32_t msr, uint32_t eax, uint32_t edx){
      __asm__ volatile ("wrmsr"
            :
            : "eax"(eax), "edx"(edx), "ecx"(msr));
}

static void readMsr(uint32_t msr, uint32_t *eax, uint32_t *edx){
   uint32_t tmpEax, tmpEdx;
      __asm__ volatile ("rdmsr"
            : "=eax"(tmpEax), "=edx"(tmpEdx)
            : "ecx"(msr));
      *eax = tmpEax;
      *edx = tmpEdx;
}
