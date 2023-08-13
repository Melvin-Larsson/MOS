COMPILER=${HOME}/opt/cross2/bin/i686-elf-gcc
PREFIX?= ${HOME}/Programmering/OS/sysroot/lib
INCLUDE?=${PREFIX}/include
CFLAGS?= -O2 -std=gnu99 -ffreestanding -mno-red-zone -Wall -Wextra -I ${INCLUDE}
BUILD?=${PREFIX}/build

LINK_FLAGS?= -nostdlib -lgcc -ffreestanding -O2 -r

OBJS?= ${BUILD}/stdio.o \
       ${BUILD}/stdlib.o \
       ${BUILD}/memory.o \
	   ${BUILD}/string.o


${BUILD}/lib.out : ${OBJS} 
	${COMPILER} ${LINK_FLAGS} ${OBJS} -o ${BUILD}/lib.out

${BUILD}/stdio.o : stdio/stdio.c include/stdio.h
	${COMPILER} ${CFLAGS} -c stdio/stdio.c -o ${BUILD}/stdio.o

${BUILD}/stdlib.o : stdlib/stdlib.c include/stdlib.h
	${COMPILER} ${CFLAGS} -c stdlib/stdlib.c -o ${BUILD}/stdlib.o
${BUILD}/memory.o : stdlib/memory.c include/stdlib.h
	${COMPILER} ${CFLAGS} -c stdlib/memory.c -o ${BUILD}/memory.o

${BUILD}/string.o : string/string.c include/string.h
	${COMPILER} ${CFLAGS} -c string/string.c -o ${BUILD}/string.o

clean :
	rm -f ${BUILD}/*
