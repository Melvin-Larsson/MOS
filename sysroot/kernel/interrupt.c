#include "kernel/interrupt.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#define GDT_CODE_SEGMENT 0x08
#define IDT_MAX_DESCRIPTIONS 256
__attribute__((aligned(0x10)))
static InterruptDescriptor interruptDescriptorTable[256];
static InterruptTableDescriptor interruptTableDescriptor;

extern void (*interrupt_addr_table[32])(void);

static void interrupt0(void){
   printf("Interrupt 0\n");
   __asm__ volatile("iret");
}
static void interrupt1(void){
   printf("Interrupt 1\n");
   __asm__ volatile("iret");
}
static void interrupt2(void){
   printf("Interrupt 2\n");
   __asm__ volatile("iret");
}
static void interrupt3(void){
   printf("Interrupt 3\n");
   __asm__ volatile("iret");
}
static void interrupt4(void){
   printf("Interrupt 4\n");
   __asm__ volatile("iret");
}
static void interrupt5(void){
   printf("Interrupt 5\n");
   __asm__ volatile("iret");
}
static void interrupt6(void){
   printf("Interrupt 6\n");
   __asm__ volatile("iret");
}
static void interrupt7(void){;
   printf("Interrupt 7\n");
   __asm__ volatile("iret");
}
static void interrupt8(void){
   printf("Interrupt 8\n");
   __asm__ volatile("iret");
}
static void interrupt9(void){
   printf("Interrupt 9\n");
   __asm__ volatile("iret");
}
static void interrupt10(void){
   printf("Interrupt 10\n");
   __asm__ volatile("iret");
}
static void interrupt11(void){
   printf("Interrupt 11\n");
   __asm__ volatile("iret");
}
static void interrupt12(void){
   printf("Interrupt 12\n");
   __asm__ volatile("iret");
}
static void interrupt13(void){
   int instruction = 69;
   __asm__ volatile("mov 0(%%esp), %0" : "=r" (instruction) :);
   int instruction2 = 69;
   __asm__ volatile("mov 0x4(%%esp), %0" : "=r" (instruction2) :);;

   char buff[10];
   char buff2[10];
   strReadInt(instruction2, buff2);
   strReadInt(instruction, buff);

   printf("Interrupt 13, error code: ");
   printf(buff);
   printf(" ,EIP: ");
   printf(buff2);
   printf("\n");

   while(1);
   __asm__ volatile("iret");
}
static void interrupt14(void){
   printf("Interrupt 14\n");
   __asm__ volatile("iret");
}
static void interrupt15(void){
   printf("Interrupt 15\n");
   __asm__ volatile("iret");
}
static void interrupt16(void){
   printf("Interrupt 16\n");
   __asm__ volatile("iret");
}
static void interrupt17(void){
   printf("Interrupt 17\n");
   __asm__ volatile("iret");
}
static void interrupt18(void){
   printf("Interrupt 18\n");
   __asm__ volatile("iret");
}
static void interrupt19(void){
   printf("Interrupt 19\n");
   __asm__ volatile("iret");
}
static void interrupt20(void){
   printf("Interrupt 20\n");
   __asm__ volatile("iret");
}
static void interrupt21(void){
   printf("Interrupt 21\n");
   __asm__ volatile("iret");
}
static void interrupt22(void){
   printf("Interrupt 22\n");
   __asm__ volatile("iret");
}
static void interrupt23(void){
   printf("Interrupt 23\n");
   __asm__ volatile("iret");
}
static void interrupt24(void){
   printf("Interrupt 24\n");
   __asm__ volatile("iret");
}
static void interrupt25(void){
   printf("Interrupt 25\n");
   __asm__ volatile("iret");
}
static void interrupt26(void){
   printf("Interrupt 26\n");
   __asm__ volatile("iret");
}
static void interrupt27(void){
   printf("Interrupt 27\n");
   __asm__ volatile("iret");
}
static void interrupt28(void){
   printf("Interrupt 28\n");
   __asm__ volatile("iret");
}
static void interrupt29(void){
   printf("Interrupt 29\n");
   __asm__ volatile("iret");
}
static void interrupt30(void){
   printf("Interrupt 30\n");
   __asm__ volatile("iret");
}
static void interrupt31(void){
   printf("Interrupt 31\n");
   __asm__ volatile("iret");
}

static void setInterruptDescriptor(uint8_t pos, void (*func)(void), uint8_t flags){
   InterruptDescriptor *desc = &interruptDescriptorTable[pos];

   desc->isrLow = (uint32_t)func & 0xFFFF;
   desc->isrHigh = (uint32_t)func >> 16;
   desc->segment = GDT_CODE_SEGMENT; 
   desc->attributes = flags;
   desc->reserved = 0;
}
void exception_handler(char interruptVector){
   printf("Interrupt! ");
   char buffer[10];
   strReadInt(interruptVector, buffer);
   printf(buffer);
   printf("\n");
   while(1);
}
void interruptDescriptorTableInit(){
   interruptTableDescriptor.base = (uint32_t)&interruptDescriptorTable[0];
   interruptTableDescriptor.limit = (uint16_t)sizeof(InterruptDescriptor) * IDT_MAX_DESCRIPTIONS - 1;
   void (*handlers[])(void) = {interrupt0,interrupt1,interrupt2,interrupt3,
                               interrupt4,interrupt5,interrupt6,interrupt7,
                               interrupt8,interrupt9,interrupt10,interrupt11,
                               interrupt12,interrupt13,interrupt14,interrupt15,
                               interrupt16,interrupt17,interrupt18,interrupt19,
                               interrupt20,interrupt21,interrupt22,interrupt23,
                               interrupt24,interrupt25,interrupt26,interrupt27,
                               interrupt28,interrupt29,interrupt30,interrupt31};

   memset(interruptDescriptorTable, 0, interruptTableDescriptor.limit);
   for(uint8_t pos = 0; pos < 32; pos++){
      setInterruptDescriptor(pos, interrupt_addr_table[pos], 0x8E);
   }
   __asm__ volatile ("lidt %0" : : "m"(interruptTableDescriptor));
   printf("Initialized interrupt table\n");
   printf("Activating interrupts:");
   __asm__ volatile ("sti");
}
