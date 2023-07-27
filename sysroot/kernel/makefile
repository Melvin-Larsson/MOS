
COMPILER=${HOME}/opt/cross2/bin/i686-elf-gcc
PREFIX?= ${HOME}/Programmering/OS/sysroot/kernel
INCLUDE=-I ${PREFIX}/include -I ${HOME}/Programmering/OS/sysroot/lib/include 
CFLAGS?= -O2 -std=gnu99 -ffreestanding -mno-red-zone -Wall -Wextra ${INCLUDE}
BUILD?=${PREFIX}/build

LINK_FLAGS?= -nostdlib -lgcc -ffreestanding -O2 -r

OBJS?= ${BUILD}/boot.o \
       ${BUILD}/kernel1.o \
	   ${BUILD}/interrupt.o

${BUILD}/kernel.o : ${OBJS} 
	${COMPILER} -T linker.ld ${LINK_FLAGS} ${OBJS} -o ${BUILD}/kernel.out

${BUILD}/kernel1.o : main.c
	${COMPILER} ${CFLAGS} -c main.c -o ${BUILD}/kernel1.o

${BUILD}/interrupt.o : interrupt.c include/kernel/interrupt.h
	${COMPILER} ${CFLAGS} -c interrupt.c -o ${BUILD}/interrupt.o

${BUILD}/boot.o :
	cd ${PREFIX}/i686 && \
	nasm -f elf program.s -o boot.o && \
	mv boot.o ${BUILD}/boot.o && \
	cd -

clean :
	rm -f ${BUILD}/*
