#include "kernel/paging.h"
#include "kernel/xhcd-ring.h"
#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"

#define DEFAULT_PCS 1

#define CRCR_OFFSET 0x18

#define TRB_TYPE_LINK 6
#define TRB_TYPE_NOOP 23
#define TRB_TYPE_ENABLE_SLOT 9
#define TRB_TYPE_ADDRESS_DEVICE 11
#define TRB_TYPE_EVALUATE_CONTEXT 13
#define TRB_TYPE_CONFIGURE_ENDPOINT 12
#define TRB_TYPE_SETUP 2
#define TRB_TYPE_DATA 3
#define TRB_TYPE_STATUS 4
#define TRB_TYPE_NORMAL 1


#define TRB_SLOT_TYPE_POS 16
#define TRB_SLOT_ID_POS 24
#define TRB_TYPE_POS 10

#define DIRECTION_IN 1
#define DIRECTION_OUT 0

#define ADDRESS_TRB_BSR_POS 9

#define TRANSFER_TYPE_IN 3

#define REQUEST_GET_DESCRIPTOR 6
#define REQUEST_SET_CONFIGURATION 9

#define DESCRIPTOR_TYPE_DEVICE 1
#define DESCRIPTOR_TYPE_CONFIGURATION 2

static void initSegment(Segment segment, Segment nextSegment, int isLast);

XhcdRing xhcd_newRing(int trbCount){
   void* ringAddress = callocco(trbCount * sizeof(TRB), 64, 64000);
   printf("ring Address %d\n", ringAddress);
   Segment segment = {(uintptr_t)ringAddress, trbCount};
   initSegment(segment, segment, 1); //FIXME: isLast = 1

   XhcdRing ring;
   ring.pcs = DEFAULT_PCS;
   ring.dequeue = (TRB *)ringAddress;
   return ring;
}
int xhcd_attachCommandRing(XhcHardware xhcHardware, XhcdRing *ring){
   uintptr_t address = paging_getPhysicalAddress((uintptr_t)ring->dequeue);
   xhcd_writeRegister(xhcHardware, CRCR, address | ring->pcs);
   return 1;
}
void xhcd_putTD(TD td, XhcdRing *ring){
   for(int i = 0; i < td.trbCount; i++){
      xhcd_putTRB(td.trbs[i], ring);
   }
}
void xhcd_putTRB(TRB trb, XhcdRing *ring){
   trb.cycleBit = ring->pcs;
   *ring->dequeue = trb; 
   ring->dequeue++;
   if(ring->dequeue->type == TRB_TYPE_LINK){
      LinkTRB *link = (LinkTRB*)ring->dequeue;
      link->cycleBit = ring->pcs;
      uintptr_t address = link->ringSegment;
      ring->dequeue = (TRB*)address;
      ring->pcs ^= link->toggleCycle;
   }
}
TRB TRB_NOOP(){
   TRB trb = {{{0,0,0,0}}};
   trb.r3 = TRB_TYPE_NOOP << TRB_TYPE_POS;
   return trb;
}
TRB TRB_ENABLE_SLOT(int slotType){
   TRB trb = {{{0,0,0,0}}};
   trb.r3 = TRB_TYPE_ENABLE_SLOT << TRB_TYPE_POS |
            slotType << TRB_SLOT_TYPE_POS;
   return trb;
}
TRB TRB_ADDRESS_DEVICE(uint64_t inputContextAddr, uint32_t slotId, uint32_t bsr){
   TRB trb = {{{0,0,0,0}}};
   trb.r0 = (uint32_t)inputContextAddr;
   trb.r1 = (uint32_t)(inputContextAddr >> 32);
   trb.r3 = bsr << ADDRESS_TRB_BSR_POS |
            TRB_TYPE_ADDRESS_DEVICE << TRB_TYPE_POS |
            slotId << TRB_SLOT_ID_POS;
   return trb;
}
TRB TRB_EVALUATE_CONTEXT(void* inputContext, uint32_t slotId){
   TRB trb = {{{0,0,0,0}}};
   trb.dataBufferPointer = (uintptr_t)inputContext;
   trb.r3 = TRB_TYPE_EVALUATE_CONTEXT << TRB_TYPE_POS |
            slotId << TRB_SLOT_ID_POS;
   return trb;
}
TRB TRB_CONFIGURE_ENDPOINT(void *inputContext, uint32_t slotId){
   TRB trb = {{{0,0,0,0}}};
   trb.dataBufferPointer = (uintptr_t)inputContext;
   trb.r3 = TRB_TYPE_CONFIGURE_ENDPOINT << TRB_TYPE_POS |
            slotId << TRB_SLOT_ID_POS;
   return trb;


}
TRB TRB_NORMAL(void *dataBuffer, uint16_t bufferSize){
   TRB trb = {{{0,0,0,0}}};
   trb.dataBufferPointer = (uintptr_t)dataBuffer;
   trb.transferLength = bufferSize;
   trb.interruptOnCompletion = 1;
   trb.interruptOnShortPacket = 1;
   trb.type = TRB_TYPE_NORMAL; 

   return trb;
}
TRB TRB_SETUP_STAGE(SetupStageHeader header){
   SetupStageTRB setupTrb = {{{0,0,0,0}}};

   setupTrb.bmRequestType = header.bmRequestType;
   setupTrb.bRequest = header.bRequest;
   setupTrb.wValue = header.wValue;
   setupTrb.wIndex = header.wIndex;
   setupTrb.wLength = header.wLength;

   setupTrb.transferLength = 8;
   setupTrb.immediateData = 1;
   setupTrb.interruptOnCompletion = 0;
   setupTrb.type = TRB_TYPE_SETUP;
   if(header.wLength == 0){
      setupTrb.transferType = NoDataStage;
   }else if(header.bmRequestType & (1<<7)){ //Device-to-host
      setupTrb.transferType = InDataStage;
   }else{
      setupTrb.transferType = OutDataStage;
   }
   TRB result;
   memcpy(&result, &setupTrb, sizeof(TRB));
   return result;
}
TRB TRB_DATA_STAGE(uint64_t dataBufferPointer, int bufferSize, uint8_t direction){
   DataStageTRB dataTrb = {{{0,0,0,0}}};

   dataTrb.dataBuffer = dataBufferPointer;
   dataTrb.transferLength = bufferSize;
   dataTrb.chainBit = 0;
   dataTrb.interruptOnCompletion = 0;
   dataTrb.immediateData = 0;
   dataTrb.type = TRB_TYPE_DATA;
   dataTrb.direction = direction;
   TRB result;
   memcpy(&result, &dataTrb, sizeof(TRB));
   return result;
}
TRB TRB_STATUS_STAGE(uint8_t direction){
   StatusStageTRB statusTrb = {{{0,0,0,0}}};
   statusTrb.interruptOnCompletion = 1;
   statusTrb.type = TRB_TYPE_STATUS;
   statusTrb.direction = direction; 
   statusTrb.chainBit = 0;

   TRB result;
   memcpy(&result, &statusTrb, sizeof(TRB));
   return result;
}
TD TD_GET_DESCRIPTOR(void *dataBufferPointer, int descriptorLength){
   SetupStageHeader setupHeader;

   setupHeader.bmRequestType = 0x80;
   setupHeader.bRequest = REQUEST_GET_DESCRIPTOR;
   setupHeader.wValue = DESCRIPTOR_TYPE_DEVICE << 8;
   setupHeader.wIndex = 0;
   setupHeader.wLength = descriptorLength;

   TRB setupTrb = TRB_SETUP_STAGE(setupHeader);
   TRB dataTrb = TRB_DATA_STAGE((uintptr_t)dataBufferPointer, descriptorLength, DIRECTION_IN);
   TRB statusTrb = TRB_STATUS_STAGE(DIRECTION_OUT);

   TD result = {{setupTrb, dataTrb, statusTrb}, 3};
   return result;
}
static void initSegment(Segment segment, Segment nextSegment, int isLast){
   memset((void*)segment.address, 0, segment.trbCount * sizeof(TRB));
   TRB *trbs = (TRB*)segment.address;
   if(!DEFAULT_PCS){
      for(int i = 0; i < segment.trbCount; i++){
         trbs[i].r3 |= 1;
      }
   }
   LinkTRB *link = (LinkTRB*)&trbs[segment.trbCount - 1];
   link->ringSegment = (uintptr_t)nextSegment.address;
   link->cycleBit = DEFAULT_PCS;
   link->toggleCycle = isLast;
   link->trbType = TRB_TYPE_LINK;
}
