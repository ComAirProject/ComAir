CXX=clang
CFLAGS=-O0 -Xclang -disable-O0-optnone -g -flto
LDFLAGS=-use-gold-plugin -Wl,-plugin-opt=save-temps
TARGET=target

PROJECT_DIR=../../../
BUILD_LIB_DIR=${PROJECT_DIR}/cmake-build-debug/lib/
RUNTIME_DIR=${PROJECT_DIR}/cmake-build-debug/runtime/
BUG_DIR=${PROJECT_DIR}/stubs/apache34464/
ID_PASS=${BUILD_LIB_DIR}/IDAssigner/libIDAssignerPass.so
MARK_PASS=${BUILD_LIB_DIR}/MarkFlagForAprof/libMarkFlagForAprofPass.so
APROF_PASS=${BUILD_LIB_DIR}/AprofHookPass/libAProfHookPass.so
INLINE_PASS=${BUILD_LIB_DIR}/MakeFunctionInline/libMakeFunctionInlinePass.so

APROF_LIB=${PROJECT_DIR}/runtime/AprofHooks/prof-hook.o
SRC_FILE=${BUG_DIR}/Telnet.c

FUNC_NAME_ID=func_name_id.txt

TARGETS_DIR=${BUG_DIR}targets
RESULTS_DIR=${BUG_DIR}results

TARGET=target
APROF=aproflib

.PHONY: all install clean

all: ${TARGET}

${TARGET}: ${TARGET}.inline.bc
	${CXX} ${CFLAGS} ${LDFLAGS} $< -lm -lrt -o $@

${TARGET}.inline.bc: ${TARGET}.pre.inline.bc ${INLINE_PASS}
	opt -load ${INLINE_PASS} -func-inline -lib-inline 0 $< > $@

${TARGET}.pre.inline.bc: ${TARGET}.inst.bc ${APROF}.inline.bc
	llvm-link $^ -o $@

${APROF}.inline.bc: ${APROF_LIB} ${INLINE_PASS}
	opt -load ${INLINE_PASS} -func-inline -lib-inline 1 $< > $@

${TARGET}.inst.bc ${FUNC_NAME_ID}: ${TARGET}.mark.id.bc ${APROF_PASS}
	opt -load ${APROF_PASS} -instrument-hooks -not-optimize 1 -strFileName ${FUNC_NAME_ID} $< > ${TARGET}.inst.bc

${TARGET}.mark.id.bc: ${TARGET}.bc.id ${MARK_PASS}
	opt -load ${MARK_PASS} -mark-flags -not-optimize 1 $< > $@

${TARGET}.bc.id: ${TARGET}.merge.bc ${ID_PASS}
	opt -load ${ID_PASS} -tag-id $< > $@

${TARGET}.merge.bc: ${TARGET}.bc
	opt -mergereturn $< > $@

${TARGET}.bc: ${SRC_FILE}
	${CXX} ${CFLAGS} ${SRC_FILE} -c -o $@

install: ${TARGET} ${FUNC_NAME_ID} ${TARGETS_DIR} ${RESULTS_DIR}
	cp ${TARGET} ${TARGETS_DIR}
	cp ${FUNC_NAME_ID} ${RESULTS_DIR}

${TARGETS_DIR}:
	mkdir -p $@

${RESULTS_DIR}:
	mkdir -p $@

clean:
	rm -rf *.o ${TARGET} ${TARGET}.opt *.bc *.resolution.txt *.id target.aprof *.ll *.opt *.bb *.txt

