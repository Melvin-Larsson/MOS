COMPILER?=gcc
PREFIX?= ${HOME}/Programmering/OS
SYSROOT=${PREFIX}/sysroot
BUILD=${PREFIX}/tests/kernel/build
KERNEL=${PREFIX}/sysroot/kernel
TESTS=${PREFIX}/tests
LIBS=${SYSROOT}/lib
MOCKS=${PREFIX}/tests/kernel/mocks

INCLUDE=-I ${PREFIX}/sysroot/kernel/include -I ${LIBS}/include -I ${PREFIX}/sysroot/kernel -I ${PREFIX}/tests -I ${MOCKS} -I ${PREFIX}/utils/include
CFLAGS?= -std=gnu99 -Wno-builtin-declaration-mismatch ${INCLUDE}

TESTS_BIN=${TESTS}/bin
TEST_LISTS=${TESTS}/testlists

OBJS = ${TESTS_BIN}/timer-test.o \
	   ${TESTS_BIN}/allocator-test.o \
	   ${TESTS_BIN}/linked-list-test.o \
	   ${TESTS_BIN}/binary-map-test.o \
	   ${TESTS_BIN}/buffered-storage-test.o \
	   ${TESTS_BIN}/fat-test.o \
	   ${TESTS_BIN}/memory-allocator-test.o \

all : ${TESTS_BIN} ${TEST_LISTS} ${OBJS}

# Timer test
${TESTS_BIN}/timer-test.o : testrunner.c ${TEST_LISTS}/timer-test-list.c ${TESTS}/kernel/timer-test.c ${KERNEL}/timer.c ${MOCKS}/memory-mock.c ${LIBS}/collection/list/linked-list.c ${LIBS}/collection/list/list.c
	gcc ${CFLAGS} ${INCLUDE} testrunner.c ${TEST_LISTS}/timer-test-list.c ${TESTS}/kernel/timer-test.c ${MOCKS}/memory-mock.c ${KERNEL}/timer.c ${LIBS}/collection/list/linked-list.c ${LIBS}/collection/list/list.c -o ${TESTS_BIN}/timer-test.o

${TEST_LISTS}/timer-test-list.c : ${TESTS}/kernel/timer-test.c
	${TESTS}/test.sh ${TESTS}/kernel/timer-test.c

# # Allocator test
${TESTS_BIN}/allocator-test.o : testrunner.c ${TEST_LISTS}/allocator-test-list.c ${TESTS}/kernel/allocator-test.c ${KERNEL}/allocator.c ${KERNEL}/memory-allocator.c ${MOCKS}/logging-mock.c ${MOCKS}/memory-mock.c
	gcc ${CFLAGS} ${INCLUDE} testrunner.c ${TEST_LISTS}/allocator-test-list.c ${TESTS}/kernel/allocator-test.c ${MOCKS}/memory-mock.c ${KERNEL}/allocator.c ${KERNEL}/memory-allocator.c ${MOCKS}/logging-mock.c -o ${TESTS_BIN}/allocator-test.o

${TEST_LISTS}/allocator-test-list.c : ${TESTS}/kernel/allocator-test.c
	${TESTS}/test.sh ${TESTS}/kernel/allocator-test.c

# Linked list test
${TESTS_BIN}/linked-list-test.o : testrunner.c ${TEST_LISTS}/linked-list-test-list.c  ${TESTS}/lib/list/linked-list-test.c ${LIBS}/collection/list/linked-list.c ${LIBS}/collection/list/intlist.c ${MOCKS}/memory-mock.c ${LIBS}/collection/int-iterator.c
	gcc ${CFLAGS} testrunner.c ${TESTS}/lib/list/linked-list-test.c ${TEST_LISTS}/linked-list-test-list.c ${MOCKS}/memory-mock.c ${LIBS}/collection/list/linked-list.c ${LIBS}/collection/list/intlist.c  ${LIBS}/collection/int-iterator.c -o ${TESTS_BIN}/linked-list-test.o


${TEST_LISTS}/linked-list-test-list.c : ${TESTS}/lib/list/linked-list-test.c
	${TESTS}/test.sh ${TESTS}/lib/list/linked-list-test.c

# Binary map test
${TESTS_BIN}/binary-map-test.o : testrunner.c ${TEST_LISTS}/binary-map-test-list.c  ${TESTS}/lib/binary-map-test.c ${LIBS}/collection/map/binary-map.c ${LIBS}/collection/map/intmap.c ${LIBS}/collection/map/map.c ${MOCKS}/memory-mock.c 
	gcc ${CFLAGS} testrunner.c ${TESTS}/lib/binary-map-test.c ${TEST_LISTS}/binary-map-test-list.c ${MOCKS}/memory-mock.c ${LIBS}/collection/map/binary-map.c ${LIBS}/collection/map/intmap.c ${LIBS}/collection/map/map.c  -o ${TESTS_BIN}/binary-map-test.o

${TEST_LISTS}/binary-map-test-list.c : ${TESTS}/lib/binary-map-test.c
	${TESTS}/test.sh ${TESTS}/lib/binary-map-test.c

# Buffered storage test
${TESTS_BIN}/buffered-storage-test.o : testrunner.c ${TEST_LISTS}/buffered-storage-test-list.c ${TESTS}/kernel/buffered-storage-test.c ${MOCKS}/memory-mock.c
	gcc ${CFLAGS} ${INCLUDE} -I ${KERNEL} testrunner.c ${TEST_LISTS}/buffered-storage-test-list.c ${TESTS}/kernel/buffered-storage-test.c ${MOCKS}/memory-mock.c -o ${TESTS_BIN}/buffered-storage-test.o

${TEST_LISTS}/buffered-storage-test-list.c : ${TESTS}/kernel/buffered-storage-test.c
	${TESTS}/test.sh ${TESTS}/kernel/buffered-storage-test.c

# Fat test
${TESTS_BIN}/fat-test.o : testrunner.c ${TEST_LISTS}/fat-test-list.c ${TESTS}/kernel/fat-test.c ${MOCKS}/memory-mock.c ${MOCKS}/buffered-storage-mock.c ${MOCKS}/mass-storage-device-mock.c ${MOCKS}/logging-mock.c ${LIBS}/string/string.c
	gcc ${CFLAGS} ${INCLUDE} -I ${KERNEL} testrunner.c ${TEST_LISTS}/fat-test-list.c ${TESTS}/kernel/fat-test.c ${MOCKS}/buffered-storage-mock.c ${MOCKS}/mass-storage-device-mock.c ${MOCKS}/memory-mock.c ${MOCKS}/logging-mock.c ${LIBS}/string/string.c -o ${TESTS_BIN}/fat-test.o

${TEST_LISTS}/fat-test-list.c : ${TESTS}/kernel/fat-test.c
	${TESTS}/test.sh ${TESTS}/kernel/fat-test.c

# Memory allocator test
${TESTS_BIN}/memory-allocator-test.o : testrunner.c ${TEST_LISTS}/memory-allocator-test-list.c ${TESTS}/kernel/memory-allocator-test.c ${KERNEL}/memory-allocator.c ${MOCKS}/logging-mock.c
	gcc ${CFLAGS} ${INCLUDE} -I ${KERNEL} testrunner.c ${TEST_LISTS}/memory-allocator-test-list.c ${TESTS}/kernel/memory-allocator-test.c ${KERNEL}/memory-allocator.c ${MOCKS}/logging-mock.c -o ${TESTS_BIN}/memory-allocator-test.o

${TEST_LISTS}/memory-allocator-test-list.c : ${TESTS}/kernel/memory-allocator-test.c
	${TESTS}/test.sh ${TESTS}/kernel/memory-allocator-test.c

#Ignoring phsypage # FIXME: Remove tests for physpage? New implementation that uses allocator, might not need testing? Or rewrite tests.
# test.o : testrunner.c ${TESTS}/kernel/physpage-test.c ${MOCKS}/memory-mock.c
# 	gcc ${CFLAGS} ${INCLUDE} -I ${KERNEL} testrunner.c ${TESTS}/kernel/physpage-test.c ${MOCKS}/memory-mock.c -o test.o

${TESTS_BIN} : 
	mkdir ${TESTS_BIN}

${TEST_LISTS} : 
	mkdir ${TEST_LISTS}

clean :
	rm -r ${TESTS_BIN}
	rm -r ${TEST_LISTS}

