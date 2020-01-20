add_executable(dumpmem
        # List your source files here.
        include/SharedMemReader.h
        include/ParseRecord.h
        src/SharedMemReader.cpp
        src/ParseRecord.cpp)

target_include_directories(dumpmem PRIVATE include)
target_link_libraries(dumpmem rt)

# Use C++11 to compile our pass (i.e., supply -std=c++11).
target_compile_features(dumpmem PRIVATE cxx_range_for cxx_auto_type)

# LLVM is (typically) built with no C++ RTTI. We need to match that;
# otherwise, we'll get linker errors about missing RTTI data.
set_target_properties(dumpmem PROPERTIES
        COMPILE_FLAGS "-O2 -fno-rtti -fPIC")