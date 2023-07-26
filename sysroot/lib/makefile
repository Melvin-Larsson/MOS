COMPILER=${HOME}/opt/cross2/bin/i686-elf-gcc
PREFIX?= ${HOME}/Programmering/OS/sysroot/lib
INCLUDE?=${PREFIX}/include
CFLAGS?= -O2 -std=gnu99 -ffreestanding -mno-red-zone -Wall -Wextra -I ${INCLUDE}
BUILD?=${PREFIX}/build

LINK_FLAGS?= -nostdlib -lgcc -ffreestanding -O2 -r

OBJS?= ${BUILD}/stdio.o

${BUILD}/lib.out : ${OBJS} 
	${COMPILER} ${LINK_FLAGS} ${OBJS} -o ${BUILD}/lib.out

${BUILD}/stdio.o :
	${COMPILER} ${CFLAGS} -c stdio/stdio.c -o ${BUILD}/stdio.o

