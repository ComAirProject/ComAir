CXX=clang
CFLAGS=-O2 -Xclang -disable-O0-optnone -flto
LDFLAGS=-lm -lrt

TARGET=AprofLogger


# AprofLogger
${TARGET}: logmf.c
	${CXX} ${CFLAGS} ${LDFLAGS} $< -o $@

clean:
	rm -rf *.o ${TARGET}
