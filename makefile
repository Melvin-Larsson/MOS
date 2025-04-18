COMPILER?=${HOME}/opt/cross2/bin/i686-elf-gcc
CFLAGS?= -O2 -std=gnu99 -ffreestanding -mno-red-zone -Wall -Wextra
PREFIX ?= ${HOME}/Programmering/OS
SYSROOT=${PREFIX}/sysroot
LIBS=${SYSROOT}/lib
KERNEL=${SYSROOT}/kernel
BUILD=${PREFIX}/build

SHELL := /bin/bash

LINK_FLAGS?= -nostdlib -lgcc -ffreestanding -O2

STAGE_1=${KERNEL}/build/boot-stage-1.o

OBJS= ${KERNEL}/build/kernel.out \
       ${LIBS}/build/lib.out

IMAGE = ${BUILD}/fat32.img

.PHONY: all
all : ${IMAGE}
	echo ${PREFIX}
	$(MAKE) kernel
	$(MAKE) lib
	${COMPILER} -T ${PREFIX}/linker.ld ${OBJS} ${LINK_FLAGS} -o ${BUILD}/os.elf
	objcopy -O binary ${BUILD}/os.elf ${BUILD}/os.bin
	dd if=${STAGE_1} of=${IMAGE} bs=1 seek=96 conv=notrunc
	printf '\x5e' | dd if=/dev/fd/0 of=${IMAGE} bs=1 seek=1 conv=notrunc
	dd if=${BUILD}/os.bin of=${IMAGE} bs=512 seek=65 conv=notrunc



${BUILD} :
	mkdir ${BUILD}

${IMAGE} : ${BUILD}
	dd if=/dev/zero of=${IMAGE} bs=512 count=70000
	mkfs.fat -F 32 ${IMAGE}
	dd if=/dev/zero of=${BUILD}/placeholder.bin bs=512 count=512
	mcopy -o -i ${IMAGE} ${BUILD}/placeholder.bin ::os.bin


.PHONY: kernel
kernel :
	make -C ${KERNEL} 
.PHONY: lib
lib	:
	make -C ${LIBS}

clean :
	rm -f ${BUILD}/*
	$(MAKE) -C ${KERNEL} clean
	$(MAKE) -C ${LIBS} clean
