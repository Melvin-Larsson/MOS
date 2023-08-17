COMPILER?=${HOME}/opt/cross2/bin/i686-elf-gcc
CFLAGS?= -O2 -std=gnu99 -ffreestanding -mno-red-zone -Wall -Wextra
PREFIX ?= ${HOME}/Programmering/OS
SYSROOT=${PREFIX}/sysroot
LIBS=${SYSROOT}/lib
KERNEL=${SYSROOT}/kernel
BUILD=${PREFIX}/build

LINK_FLAGS?= -nostdlib -lgcc -ffreestanding -O2

OBJS= ${KERNEL}/build/kernel.out \
       ${LIBS}/build/lib.out

.PHONY: all
all:
	echo ${PREFIX}
	$(MAKE) kernel
	$(MAKE) lib
	${COMPILER} -T ${PREFIX}/linker.ld ${OBJS} ${LINK_FLAGS}  -o ${BUILD}/os.elf
	objcopy -O binary ${BUILD}/os.elf ${BUILD}/os.bin

.PHONY: kernel
kernel :
	make -C ${KERNEL} 
.PHONY: lib
lib	:
	make -C ${LIBS}

clean :
	rm -f ${BUILD}/*
	dd if=/dev/zero of=${BUILD}/os.img bs=512 count=2048
	$(MAKE) -C ${KERNEL} clean
	$(MAKE) -C ${LIBS} clean
