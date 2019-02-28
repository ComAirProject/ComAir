# In-hosue 

Implementation of In-house version

## 1. Compile hooks & config project dir

```
cd ${PROJECT_DIR}
mkdir cmake-build-debug
cd cmake-build-debug/
cmake ..
make

cd ${PROJECT_DIR}/runtime/AprofHooks/
make

cd ${PROJECT_DIR}/runtime/AprofLogger/
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
The result is in results/outer_func.csv
