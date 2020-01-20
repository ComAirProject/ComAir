add_executable(ProdRunLogger
        # List your source files here.
        SharedMemReader.h
        ParseRecord.h
        SharedMemReader.cpp
        ParseRecord.cpp)

target_include_directories(ProdRunLogger PRIVATE include)
target_link_libraries(ProdRunLogger rt)

# Use C++11 to compile our pass (i.e., supply -std=c++11).
target_compile_features(ProdRunLogger PRIVATE cxx_range_for cxx_auto_type)

# LLVM is (typically) built with no C++ RTTI. We need to match that;
# otherwise, we'll get linker errors about missing RTTI data.
set_target_properties(ProdRunLogger PROPERTIES
        COMPILE_FLAGS "-O2 -fno-rtti -fPIC")