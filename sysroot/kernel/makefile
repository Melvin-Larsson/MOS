COMPILER?=${HOME}/opt/cross2/bin/i686-elf-gcc
PREFIX?= ${HOME}/Programmering/OS
INCLUDE=-I ${PREFIX}/sysroot/kernel/include -I ${PREFIX}/sysroot/lib/include 
CFLAGS?= -O2 -std=gnu99 -ffreestanding -mno-red-zone -Wall -Wextra ${INCLUDE}
BUILD=${PREFIX}/sysroot/kernel/build

LINK_FLAGS?= -nostdlib -lgcc -ffreestanding -O2 -r

OBJS= ${BUILD}/boot.o \
       ${BUILD}/kernel1.o \
	   ${BUILD}/interrupt.o \
	   ${BUILD}/registers.o \
	   ${BUILD}/apic.o \
	   ${BUILD}/pci.o \
	   ${BUILD}/xhcd.o \
	   ${BUILD}/xhcd-ring.o \
	   ${BUILD}/xhcd-event-ring.o \
	   ${BUILD}/keyboard.o \
	   ${BUILD}/usb-mass-storage.o \
	   ${BUILD}/usb.o \
	   ${BUILD}/scsi.o \
	   ${BUILD}/mass-storage.o \
	   ${BUILD}/fat.o \

${BUILD}/kernel.o : ${BUILD} ${OBJS} 
	${COMPILER} -T linker.ld ${LINK_FLAGS} ${OBJS} -o ${BUILD}/kernel.out

${BUILD} : 
	mkdir ${BUILD}

${BUILD}/kernel1.o : main.c
	${COMPILER} ${CFLAGS} -c main.c -o ${BUILD}/kernel1.o

${BUILD}/interrupt.o : interrupt.c include/kernel/interrupt.h
	${COMPILER} ${CFLAGS} -c interrupt.c -o ${BUILD}/interrupt.o
	
${BUILD}/registers.o : registers.c include/kernel/registers.h
	${COMPILER} ${CFLAGS} -c registers.c -o ${BUILD}/registers.o
	
${BUILD}/apic.o : apic.c include/kernel/apic.h
	${COMPILER} ${CFLAGS} -c apic.c -o ${BUILD}/apic.o

${BUILD}/pci.o : pci.c include/kernel/pci.h
	${COMPILER} ${CFLAGS} -c pci.c -o ${BUILD}/pci.o



${BUILD}/xhcd.o : xhcd.c include/kernel/xhcd.h include/kernel/usb-descriptors.h
	${COMPILER} ${CFLAGS} -c xhcd.c -o ${BUILD}/xhcd.o

${BUILD}/xhcd-ring.o : xhcd-ring.c include/kernel/xhcd-ring.h
	${COMPILER} ${CFLAGS} -c xhcd-ring.c -o ${BUILD}/xhcd-ring.o

${BUILD}/xhcd-event-ring.o : xhcd-event-ring.c include/kernel/xhcd-event-ring.h
	${COMPILER} ${CFLAGS} -c xhcd-event-ring.c -o ${BUILD}/xhcd-event-ring.o

${BUILD}/usb.o : usb.c include/kernel/usb.h
	${COMPILER} ${CFLAGS} -c usb.c -o ${BUILD}/usb.o

${BUILD}/keyboard.o : keyboard.c include/kernel/keyboard.h
	${COMPILER} ${CFLAGS} -c keyboard.c -o ${BUILD}/keyboard.o
	
${BUILD}/usb-mass-storage.o : usb-mass-storage.c include/kernel/usb-mass-storage.h
	${COMPILER} ${CFLAGS} -c usb-mass-storage.c -o ${BUILD}/usb-mass-storage.o

${BUILD}/scsi.o : scsi.c include/kernel/scsi.h
	${COMPILER} ${CFLAGS} -c scsi.c -o ${BUILD}/scsi.o

${BUILD}/mass-storage.o : mass-storage.c usb-mass-storage.c include/kernel/usb-mass-storage.h include/kernel/mass-storage.h
	${COMPILER} ${CFLAGS} -c mass-storage.c -o ${BUILD}/mass-storage.o

${BUILD}/fat.o : fat.c include/kernel/fat.h
	${COMPILER} ${CFLAGS} -c fat.c -o ${BUILD}/fat.o

include/kernel/xhcd.h: include/kernel/xhcd-registers.h include/kernel/usb-messages.h
include/kernel/xhcd-ring.h: include/kernel/xhcd-registers.h
include/kernel/xhcd-event-ring.h: include/kernel/xhcd-registers.h
include/kernel/usb.h : include/kernel/usb-descriptors.h include/kernel/xhcd.h include/kernel/usb-messages.h
include/kernel/usb-mass-storage.h: include/kernel/usb.h

${BUILD}/boot.o :
	cd ${PREFIX}/sysroot/kernel/i686 && \
	nasm -f elf program.s -o boot.o && \
	mv boot.o ${BUILD}/boot.o && \
	cd -

clean :
	rm -f ${BUILD}/*
