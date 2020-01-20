# Aprof

Implementation of Aprof

## 1. Compile hooks & config project dir

```
cd ${PROJECT_DIR}
mkdir cmake-build-debug
cd cmake-build-debug/
cmake ..
make

cd ${PROJECT_DIR}/runtime/AprofHooks/
make

cd ${ROJECT_DIR}/stubs/
./config.sh
```

## 2. Instrument demo

```
cd ${PROJECT_DIR}/stubs/apache34464/
./run_exe_time.py
```

1. func_id_name.txt: map from func id to func name
2. mem_result.csv: func_id,rms,cost,chains
3. runtime_result.csv: runtime ms
