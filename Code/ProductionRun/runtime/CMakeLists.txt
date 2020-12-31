add_library(RuntimeLib STATIC
        # List your source files here.
        src/Random.c
        src/Shmem.c
        include/Random.h
        include/Shmem.h
        )

target_include_directories(RuntimeLib PRIVATE include)

# Use C++11 to compile our pass (i.e., supply -std=c++11).
target_compile_features(RuntimeLib PRIVATE cxx_range_for cxx_auto_type)

# LLVM is (typically) built with no C++ RTTI. We need to match that;
# otherwise, we'll get linker errors about missing RTTI data.
set_target_properties(RuntimeLib PROPERTIES
        COMPILE_FLAGS "-g -O2 -fno-rtti -fPIC")