# Production-run

Implementation of Production-run version

## 1. Compile

```
cd ${PROJECT_DIR}
mkdir cmake-build-debug
cd cmake-build-debug/
cmake ..
make
```

## 2. Instrument demo

```
cd ${PROJECT_DIR}/stubs/apache34464/
./gen_input.py
mkdir build
cd build
make -f ../Makefile install
cd ..
./run.sh
```

The result is in results/inner_loop.csv
