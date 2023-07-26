COMPILER=${HOME}/opt/cross2/bin/i686-elf-gcc
CFLAGS?= -O2 -std=gnu99 -ffreestanding -mno-red-zone -Wall -Wextra
PREFIX?= ${HOME}/Programmering/OS
SYSROOT?=${PREFIX}/sysroot
LIBS?=${SYSROOT}/lib
KERNEL?=${SYSROOT}/kernel
BUILD?=${PREFIX}/build

LINK_FLAGS?= -nostdlib -lgcc -ffreestanding -O2

OBJS?= ${KERNEL}/build/kernel.out \
       ${LIBS}/build/lib.out

${BUILD}/os.bin : ${OBJS} 
	${COMPILER} -T ${PREFIX}/linker.ld ${OBJS} ${LINK_FLAGS}  -o ${BUILD}/os.elf
	objcopy -O binary ${BUILD}/os.elf ${BUILD}/os.bin

${KERNEL}/build/kernel.out :
	make -C ${KERNEL} 
${LIBS}/build/lib.out :
	make -C ${LIBS}


