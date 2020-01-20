# compile
CXX=clang
OP_CFLAGS=-O0 -Xclang -disable-O0-optnone -g -flto
CFLAGS=-O0 -Xclang -disable-O0-optnone -g -flto
LDFLAGS=-use-gold-plugin -Wl,-plugin-opt=save-temps
TARGET=target
# lib path
PROJECT_DIR=../../../
LIB_DIR=${PROJECT_DIR}cmake-build-debug/
BIN_DIR=${PROJECT_DIR}bin/
INDVAR_FIND_PASS=${LIB_DIR}lib/LoopSampler/IndvarFinder/libIndvarFindPass.so
ID_PASS=${LIB_DIR}lib/IDAssigner/libIDAssignerPass.so
#LOOP_INSTRUMENT_PASS=${LIB_DIR}lib/LoopSampler/NewLoopInstrumentor/libNewLoopInstrumentPass.so
LOOP_INSTRUMENT_PASS=${LIB_DIR}lib/LoopSampler/OptLoopInstrumentor/libOptLoopInstrumentPass.so
RUNTIME_LIB=${LIB_DIR}runtime/libRuntimeLib.a
# bug fixed parts
INDVAR_INFO=indvar.pre.info
FUNC_NAME_ID=func_name_id.txt
TARGET=target

# bug specific (mutable)
BUG_DIR=${PROJECT_DIR}stubs/apache34464/
SRC_FILE=${BUG_DIR}Telnet.c
LINENO=103
FUNCNAME=indexOf

# target and result
TARGET_DIR=${BUG_DIR}targets
RESULT_DIR=${BUG_DIR}results

.PHONY: all install clean

all: ${TARGET} ${ORIGIN_TARGET}

${TARGET}: ${TARGET}.lalls.bc ${RUNTIME_LIB}
	${CXX} ${OP_CFLAGS} ${LDFLAGS} $< ${RUNTIME_LIB} -lm -lrt -o $@

${TARGET}.lalls.bc: ${TARGET}.bc.id ${LOOP_INSTRUMENT_PASS} ${SRC_FILE} ${INDVAR_INFO}
	opt -load ${LOOP_INSTRUMENT_PASS} -opt-loop-instrument \
	 -noLine ${LINENO} -strFile ${SRC_FILE} \
	 -strFunc ${FUNCNAME} $< > $@

${INDVAR_INFO}: ${TARGET}.m2r.bc ${INDVAR_FIND_PASS}
	opt -load ${INDVAR_FIND_PASS} -indvar-find -noLine ${LINENO} -strFile ${SRC_FILE} -strFunc ${FUNCNAME} $< > /dev/null

${TARGET}.m2r.bc: ${TARGET}.bc.id
	opt -mem2reg $< > $@

${TARGET}.bc.id: ${TARGET}.ls.bc ${ID_PASS}
	opt -load ${ID_PASS} -tag-id $< > $@

${TARGET}.ls.bc: ${ORIGIN_TARGET}.bc
	opt -loop-simplify $< > $@

${ORIGIN_TARGET}.bc: ${SRC_FILE}
	${CXX} ${CFLAGS} ${SRC_FILE} -c -o $@

install: ${TARGET} ${ORIGIN_TARGET} ${FUNC_NAME_ID} ${TARGET_DIR} ${RESULT_DIR}
	cp ${TARGET} ${ORIGIN_TARGET} ${TARGET_DIR}
	cp ${FUNC_NAME_ID} ${RESULT_DIR} 
	cp indvar.info ${RESULT_DIR}

${TARGET_DIR}:
	mkdir -p $@

${RESULT_DIR}:
	mkdir -p $@

clean:
	rm -rf *.o ${TARGET}.* *.bc *.resolution.txt *.id target.aprof *.ll *.opt *.bb *.txt

