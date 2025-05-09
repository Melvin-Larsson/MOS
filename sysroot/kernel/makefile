COMPILER?=${HOME}/opt/cross2/bin/i686-elf-gcc
PREFIX?= ${HOME}/Programmering/OS
INCLUDE=-I ${PREFIX}/sysroot/kernel/include -I ${PREFIX}/sysroot/lib/include -I ${PREFIX}/utils/include
CFLAGS?= -O2 -std=gnu99 -ffreestanding -mno-red-zone -DLOG_LEVEL=0 -DASSERTS_ENABLED -Wall -Wextra -Werror --param=min-pagesize=0 ${INCLUDE}
BUILD=${PREFIX}/sysroot/kernel/build

LINK_FLAGS?= -nostdlib -lgcc -ffreestanding -O2 -r

BOOT_LOADER=${PREFIX}/sysroot/kernel/i686


OBJS=  ${BUILD}/boot-stage-2.o \
       ${BUILD}/kernel1.o \
	   ${BUILD}/interrupt.o \
	   ${BUILD}/registers.o \
	   ${BUILD}/apic.o \
	   ${BUILD}/pci.o \
	   ${BUILD}/xhcd.o \
	   ${BUILD}/xhcd-hardware.o \
	   ${BUILD}/xhcd-ring.o \
	   ${BUILD}/xhcd-event-ring.o \
	   ${BUILD}/keyboard.o \
	   ${BUILD}/usb-mass-storage.o \
	   ${BUILD}/usb.o \
	   ${BUILD}/scsi.o \
	   ${BUILD}/mass-storage.o \
	   ${BUILD}/fat.o \
	   ${BUILD}/fat-disk.o \
	   ${BUILD}/file-system.o \
	   ${BUILD}/buffered-storage.o \
	   ${BUILD}/task.o \
	   ${BUILD}/descriptors.o \
	   ${BUILD}/paging.o \
	   ${BUILD}/physpage.o \
	   ${BUILD}/allocator.o \
	   ${BUILD}/serial.o \
	   ${BUILD}/logging.o \
	   ${BUILD}/kernel-io.o \
	   ${BUILD}/pit.o \
	   ${BUILD}/ioport.o \
	   ${BUILD}/ioapic.o \
	   ${BUILD}/acpi.o \
	   ${BUILD}/threads.o \
	   ${BUILD}/timer.o \
	   ${BUILD}/memory.o \
	   ${BUILD}/ps2-8042.o \

.PHONY: all
all: ${BUILD}/kernel.o ${BUILD}/boot-stage-1.o

${BUILD}/kernel.o : ${BUILD} ${OBJS} ${PREFIX}/utils/include/utils/assert.h
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

${BUILD}/pci.o : pci.c include/kernel/pci.h include/kernel/msix-structures.h
	${COMPILER} ${CFLAGS} -c pci.c -o ${BUILD}/pci.o



${BUILD}/xhcd.o : xhcd.c include/kernel/xhcd.h include/kernel/usb-descriptors.h
	${COMPILER} ${CFLAGS} -c xhcd.c -o ${BUILD}/xhcd.o

${BUILD}/xhcd-ring.o : xhcd-ring.c include/kernel/xhcd-ring.h
	${COMPILER} ${CFLAGS} -c xhcd-ring.c -o ${BUILD}/xhcd-ring.o

${BUILD}/xhcd-event-ring.o : xhcd-event-ring.c include/kernel/xhcd-event-ring.h
	${COMPILER} ${CFLAGS} -c xhcd-event-ring.c -o ${BUILD}/xhcd-event-ring.o

${BUILD}/xhcd-hardware.o : xhcd-hardware.c include/kernel/xhcd-hardware.h
	${COMPILER} ${CFLAGS} -c xhcd-hardware.c -o ${BUILD}/xhcd-hardware.o

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

${BUILD}/fat.o : fat.c include/kernel/fat.h include/kernel/mass-storage.h
	${COMPILER} ${CFLAGS} -c fat.c -o ${BUILD}/fat.o

${BUILD}/file-system.o : file-system.c include/kernel/file-system.h
	${COMPILER} ${CFLAGS} -c file-system.c -o ${BUILD}/file-system.o

${BUILD}/buffered-storage.o : buffered-storage.c include/kernel/buffered-storage.h
	${COMPILER} ${CFLAGS} -c buffered-storage.c -o ${BUILD}/buffered-storage.o

${BUILD}/fat-disk.o : fat-disk.c include/kernel/fat-disk.h
	${COMPILER} ${CFLAGS} -c fat-disk.c -o ${BUILD}/fat-disk.o

${BUILD}/task.o : task.c include/kernel/task.h include/kernel/task-structures.h
	${COMPILER} ${CFLAGS} -c task.c -o ${BUILD}/task.o

${BUILD}/descriptors.o : descriptors.c include/kernel/descriptors.h
	${COMPILER} ${CFLAGS} -c descriptors.c -o ${BUILD}/descriptors.o

${BUILD}/paging.o : paging.c include/kernel/paging.h
	${COMPILER} ${CFLAGS} -c paging.c -o ${BUILD}/paging.o

${BUILD}/physpage.o : physpage.c include/kernel/physpage.h
	${COMPILER} ${CFLAGS} -c physpage.c -o ${BUILD}/physpage.o

${BUILD}/allocator.o : allocator.c include/kernel/allocator.h
	${COMPILER} ${CFLAGS} -c allocator.c -o ${BUILD}/allocator.o

${BUILD}/serial.o : serial.c include/kernel/serial.h
	${COMPILER} ${CFLAGS} -c serial.c -o ${BUILD}/serial.o

${BUILD}/logging.o : logging.c include/kernel/logging.h
	${COMPILER} ${CFLAGS} -c logging.c -o ${BUILD}/logging.o

${BUILD}/kernel-io.o : kernel-io.c include/kernel/kernel-io.h
	${COMPILER} ${CFLAGS} -c kernel-io.c -o ${BUILD}/kernel-io.o

${BUILD}/pit.o : pit.c include/kernel/pit.h
	${COMPILER} ${CFLAGS} -c pit.c -o ${BUILD}/pit.o

${BUILD}/ioport.o : ioport.c include/kernel/ioport.h
	${COMPILER} ${CFLAGS} -c ioport.c -o ${BUILD}/ioport.o

${BUILD}/ioapic.o : ioapic.c include/kernel/ioapic.h
	${COMPILER} ${CFLAGS} -c ioapic.c -o ${BUILD}/ioapic.o

${BUILD}/acpi.o : acpi.c include/kernel/acpi.h
	${COMPILER} ${CFLAGS} -c acpi.c -o ${BUILD}/acpi.o

${BUILD}/threads.o : threads.c include/kernel/threads.h
	${COMPILER} ${CFLAGS} -c threads.c -o ${BUILD}/threads.o

${BUILD}/timer.o : timer.c include/kernel/timer.h
	${COMPILER} ${CFLAGS} -c timer.c -o ${BUILD}/timer.o

${BUILD}/memory.o : memory.c include/kernel/memory.h
	${COMPILER} ${CFLAGS} -c memory.c -o ${BUILD}/memory.o

${BUILD}/ps2-8042.o : ps2-8042.c include/kernel/ps2-8042.h include/kernel/ps2-8042-structures.h
	${COMPILER} ${CFLAGS} -c ps2-8042.c -o ${BUILD}/ps2-8042.o

include/kernel/xhcd.h: include/kernel/xhcd-registers.h include/kernel/usb-messages.h
include/kernel/xhcd-ring.h: include/kernel/xhcd-registers.h
include/kernel/xhcd-event-ring.h: include/kernel/xhcd-registers.h
include/kernel/usb.h : include/kernel/usb-descriptors.h include/kernel/xhcd.h include/kernel/usb-messages.h
include/kernel/usb-mass-storage.h: include/kernel/usb.h

${BUILD}/boot-stage-1.o : ${BOOT_LOADER}/stage1
	nasm -i ${BOOT_LOADER} -i ${BOOT_LOADER}/stage1 ${BOOT_LOADER}/stage1/program.s -o ${BUILD}/boot-stage-1.o

${BUILD}/boot-stage-2.o : ${BOOT_LOADER}/stage2
	nasm -f elf -i ${BOOT_LOADER}/stage2 -i ${BOOT_LOADER} ${BOOT_LOADER}/stage2/program.s -o ${BUILD}/boot-stage-2.o

clean :
	rm -f ${BUILD}/*
