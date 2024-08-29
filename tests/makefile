SUBJECT_FILE=linked-list
SUBJECT_PARENT=lib/list
SUBJECT=${SUBJECT_PARENT}/${SUBJECT_FILE}

COMPILER?=${HOME}/opt/cross2/bin/i686-elf-gcc
PREFIX?= ${HOME}/Programmering/OS
SYSROOT=${PREFIX}/sysroot
BUILD=${PREFIX}/tests/kernel/build
KERNEL=${PREFIX}/sysroot/kernel
TESTS=${PREFIX}/tests
LIBS=${SYSROOT}/lib
MOCKS=${PREFIX}/tests/kernel/mocks

SUBJECT_SOURCE=${SYSROOT}/${SUBJECT}.c
TEST_FILE=${SUBJECT}-test.c

INCLUDE=-I ${PREFIX}/sysroot/kernel/include -I ${PREFIX}/sysroot/lib/include -I ${PREFIX}/sysroot/kernel -I ${PREFIX}/tests -I ${MOCKS} -I ${PREFIX}/utils/include
CFLAGS?= -O2 -std=gnu99 -ffreestanding -mno-red-zone -Wall -Wextra ${INCLUDE}

LINK_FLAGS?= -nostdlib -lgcc -ffreestanding -O2

OBJS = ${BUILD}/boot.o \
	   ${BUILD}/test.o \
	   ${BUILD}/testrunner.o \
	   ${BUILD}/interrupt.o \
	   ${BUILD}/logging.o \
	   ${BUILD}/kernel-io.o \
	   ${BUILD}/mass-storage-device-mock.o \
	   ${BUILD}/buffered-storage-mock.o \
# 	   ${BUILD}/${SUBJECT_FILE}.o \

all : ${PREFIX}/build/os.img ${OBJS}
	echo ${PREFIX}
	$(MAKE) lib
	${COMPILER} -T ${PREFIX}/linker.ld ${OBJS} ${LIBS}/build/lib.out ${LINK_FLAGS}  -o ${PREFIX}/build/os.elf
	objcopy -O binary ${PREFIX}/build/os.elf ${PREFIX}/build/os.bin


${PREFIX}/build/os.img : ${PREFIX}/build ${OBJS}
	dd if=/dev/zero of=${PREFIX}/build/os.img bs=512 count=2048

${BUILD} : 
	mkdir ${BUILD}

${PREFIX}/build : 
	mkdir ${PREFIX}/build

${BUILD}/test.o : ${TESTS}/${TEST_FILE} ${PREFIX}/tests/testrunner.h ${BUILD};
	${COMPILER} ${CFLAGS} -c ${TESTS}/${TEST_FILE} -o ${BUILD}/test.o

${BUILD}/testrunner.o : ${PREFIX}/tests/testrunner.c ${PREFIX}/tests/testrunner.h ${BUILD};
	${COMPILER} ${CFLAGS} -c ${PREFIX}/tests/testrunner.c -o ${BUILD}/testrunner.o
	
${BUILD}/interrupt.o : ${KERNEL}/interrupt.c ${BUILD};
	${COMPILER} ${CFLAGS} -c ${KERNEL}/interrupt.c -o ${BUILD}/interrupt.o

${BUILD}/logging.o : ${KERNEL}/logging.c ${KERNEL}/include/kernel/logging.h
	${COMPILER} ${CFLAGS} -c ${KERNEL}/logging.c -o ${BUILD}/logging.o

${BUILD}/kernel-io.o : ${KERNEL}/kernel-io.c ${KERNEL}/include/kernel/kernel-io.h
	${COMPILER} ${CFLAGS} -c ${KERNEL}/kernel-io.c -o ${BUILD}/kernel-io.o

${BUILD}/mass-storage-device-mock.o : ${MOCKS}/mass-storage-device-mock.c ${BUILD};
	${COMPILER} ${CFLAGS} -c ${MOCKS}/mass-storage-device-mock.c -o ${BUILD}/mass-storage-device-mock.o

${BUILD}/buffered-storage-mock.o : ${MOCKS}/buffered-storage-mock.c ${BUILD};
	${COMPILER} ${CFLAGS} -c ${MOCKS}/buffered-storage-mock.c -o ${BUILD}/buffered-storage-mock.o

${BUILD}/boot.o : ${PREFIX}/sysroot/kernel/i686 ${BUILD}
	cd ${PREFIX}/sysroot/kernel/i686 && \
	nasm -f elf program.s -o boot.o && \
	mv boot.o ${BUILD}/boot.o && \
	cd -

${BUILD}/${SUBJECT_FILE}.o : ${SUBJECT_SOURCE} ${KERNEL}/include/kernel/allocator.h;
	${COMPILER} ${CFLAGS} -c ${SUBJECT_SOURCE} -o ${BUILD}/${SUBJECT_FILE}.o

.PHONY: lib
lib :
	make -C ${LIBS}

.PHONY: kernel
kernel :
	make -C ${KERNEL}
