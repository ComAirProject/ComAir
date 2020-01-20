CC=clang++ -std=c++11
#CFLAGS=-O0 -Xclang -disable-O0-optnone -g -flto
CFLAGS=-O0 -Xclang -disable-O0-optnone -flto
#CFLAGS=-O2 -flto
TARGET=InHouseETLHooks
PROJECT_DIR=../../
BUILD_LIB_DIR=${PROJECT_DIR}cmake-build-debug/lib/
INLINE_PASS=${BUILD_LIB_DIR}FuncInlinePass/libFuncInlinePass.so

.PHONY: clean

${TARGET}.inline.bc: ${TARGET}.bc
	opt -load ${INLINE_PASS} -func-inline -lib-inline 1 $< > $@

${TARGET}.bc: InHouseETLHooks.cpp InHouseETLHooks.h
	${CC} ${CFLAGS} -c $< -o $@

clean:
	rm ${TARGET}.bc ${TARGET}.inline.bc
