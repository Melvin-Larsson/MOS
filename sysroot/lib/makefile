COMPILER?=${HOME}/opt/cross2/bin/i686-elf-gcc
PREFIX?= ${HOME}/Programmering/OS
INCLUDE=${PREFIX}/sysroot/lib/include
CFLAGS?= -O2 -std=gnu99 -ffreestanding -mno-red-zone -Wall -Wextra -I ${INCLUDE}
BUILD=${PREFIX}/sysroot/lib/build

LINK_FLAGS?= -nostdlib -lgcc -ffreestanding -O2 -r

OBJS= ${BUILD}/stdio.o \
       ${BUILD}/stdlib.o \
       ${BUILD}/memory.o \
	   ${BUILD}/string.o \
	   ${BUILD}/map.o \
	   ${BUILD}/binary-map.o \
	   ${BUILD}/intmap.o \

${BUILD}/lib.out : ${BUILD} ${OBJS} 
	${COMPILER} ${LINK_FLAGS} ${OBJS} -o ${BUILD}/lib.out

${BUILD} :
	mkdir ${BUILD}

${BUILD}/stdio.o : stdio/stdio.c include/stdio.h
	${COMPILER} ${CFLAGS} -c stdio/stdio.c -o ${BUILD}/stdio.o

${BUILD}/stdlib.o : stdlib/stdlib.c include/stdlib.h
	${COMPILER} ${CFLAGS} -c stdlib/stdlib.c -o ${BUILD}/stdlib.o
${BUILD}/memory.o : stdlib/memory.c include/stdlib.h
	${COMPILER} ${CFLAGS} -c stdlib/memory.c -o ${BUILD}/memory.o

${BUILD}/string.o : string/string.c include/string.h
	${COMPILER} ${CFLAGS} -c string/string.c -o ${BUILD}/string.o

${BUILD}/map.o : map/map.c include/map.h
	${COMPILER} ${CFLAGS} -c map/map.c -o ${BUILD}/map.o

${BUILD}/binary-map.o : map/binary-map.c include/map.h
	${COMPILER} ${CFLAGS} -c map/binary-map.c -o ${BUILD}/binary-map.o

${BUILD}/intmap.o : map/intmap.c include/map.h
	${COMPILER} ${CFLAGS} -c map/intmap.c -o ${BUILD}/intmap.o

clean :
	rm -f ${BUILD}/*
