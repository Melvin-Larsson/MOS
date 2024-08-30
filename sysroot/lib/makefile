COMPILER?=${HOME}/opt/cross2/bin/i686-elf-gcc
PREFIX?= ${HOME}/Programmering/OS
INCLUDE=-I ${PREFIX}/sysroot/lib/include -I ${PREFIX}/sysroot/kernel/include
CFLAGS?= -O2 -std=gnu99 -ffreestanding -mno-red-zone -Wall -Wextra ${INCLUDE}
BUILD=${PREFIX}/sysroot/lib/build

LINK_FLAGS?= -nostdlib -lgcc -ffreestanding -O2 -r

OBJS= ${BUILD}/stdio.o \
       ${BUILD}/stdlib.o \
	   ${BUILD}/string.o \
	   ${BUILD}/map.o \
	   ${BUILD}/binary-map.o \
	   ${BUILD}/intmap.o \
	   ${BUILD}/linked-list.o \
	   ${BUILD}/list.o \
	   ${BUILD}/intlist.o \
	   ${BUILD}/int-iterator.o \

${BUILD}/lib.out : ${BUILD} ${OBJS} 
	${COMPILER} ${LINK_FLAGS} ${OBJS} -o ${BUILD}/lib.out

${BUILD} :
	mkdir ${BUILD}

${BUILD}/stdio.o : stdio/stdio.c include/stdio.h
	${COMPILER} ${CFLAGS} -c stdio/stdio.c -o ${BUILD}/stdio.o

${BUILD}/stdlib.o : stdlib/stdlib.c include/stdlib.h
	${COMPILER} ${CFLAGS} -c stdlib/stdlib.c -o ${BUILD}/stdlib.o

${BUILD}/string.o : string/string.c include/string.h
	${COMPILER} ${CFLAGS} -c string/string.c -o ${BUILD}/string.o

${BUILD}/map.o : collection/map/map.c include/collection/map.h
	${COMPILER} ${CFLAGS} -c collection/map/map.c -o ${BUILD}/map.o

${BUILD}/binary-map.o : collection/map/binary-map.c include/collection/map.h
	${COMPILER} ${CFLAGS} -c collection/map/binary-map.c -o ${BUILD}/binary-map.o

${BUILD}/intmap.o : collection/map/intmap.c include/collection/map.h
	${COMPILER} ${CFLAGS} -c collection/map/intmap.c -o ${BUILD}/intmap.o

${BUILD}/list.o : collection/list/list.c include/collection/list.h
	${COMPILER} ${CFLAGS} -c collection/list/list.c -o ${BUILD}/list.o

${BUILD}/linked-list.o : collection/list/linked-list.c include/collection/list.h
	${COMPILER} ${CFLAGS} -c collection/list/linked-list.c -o ${BUILD}/linked-list.o

${BUILD}/intlist.o : collection/list/intlist.c include/collection/list.h
	${COMPILER} ${CFLAGS} -c collection/list/intlist.c -o ${BUILD}/intlist.o

${BUILD}/int-iterator.o : collection/int-iterator.c include/collection/int-iterator.h
	${COMPILER} ${CFLAGS} -c collection/int-iterator.c -o ${BUILD}/int-iterator.o

clean :
	rm -f ${BUILD}/*
