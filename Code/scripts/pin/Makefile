
TOOL_NAME=branch
PIN_HOME=/home/comair/stubs/scripts/pin/pin-3.7-97619-g0d0c92f4f-gcc-linux

CC=gcc
CXX=g++

CXX_FLAGS=-Wall -Werror -Wno-unknown-pragmas -D__PIN__=1 -DPIN_CRT=1 -fno-stack-protector -fno-exceptions -funwind-tables -fasynchronous-unwind-tables -fno-rtti -DTARGET_IA32E -DHOST_IA32E -fPIC -DTARGET_LINUX -fabi-version=2

CXX_INCLUDES=-I${PIN_HOME}/source/include/pin -I${PIN_HOME}/source/include/pin/gen -isystem ${PIN_HOME}/extras/stlport/include -isystem ${PIN_HOME}/extras/libstdc++/include -isystem ${PIN_HOME}/extras/crt/include -isystem ${PIN_HOME}/extras/crt/include/arch-x86_64 -isystem ${PIN_HOME}/extras/crt/include/kernel/uapi -isystem ${PIN_HOME}/extras/crt/include/kernel/uapi/asm-x86 -I${PIN_HOME}/extras/components/include -I${PIN_HOME}/extras/xed-intel64/include/xed -I${PIN_HOME}/source/tools/InstLib -O3 -fomit-frame-pointer -fno-strict-aliasing 

LINK_FLAGS=-shared -Wl,--hash-style=sysv ${PIN_HOME}/intel64/runtime/pincrt/crtbeginS.o -Wl,-Bsymbolic -Wl,--version-script=${PIN_HOME}/source/include/pin/pintool.ver -fabi-version=2

LIBRARY=-L${PIN_HOME}/intel64/runtime/pincrt -L${PIN_HOME}/intel64/lib -L${PIN_HOME}/intel64/lib-ext -L${PIN_HOME}/extras/xed-intel64/lib -lpin -lxed ${PIN_HOME}/intel64/runtime/pincrt/crtendS.o -lpin3dwarf  -ldl-dynamic -nostdlib -lstlport-dynamic -lm-dynamic -lc-dynamic -lunwind-dynamic


all:
	${CXX} ${CXX_FLAGS} ${CXX_INCLUDES} ${TOOL_NAME}.cpp -c -o ${TOOL_NAME}.o

	${CXX} ${LINK_FLAGS} -o ${TOOL_NAME}.so ${TOOL_NAME}.o ${LIBRARY}


clean:
	-rm -rf *.o *.out *.tested *.failed *.d *makefile.copy *.exp *.lib *.so
