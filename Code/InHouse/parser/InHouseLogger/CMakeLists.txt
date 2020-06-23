add_executable(InHouseLogger
        # List your source files here.
        InHouseLogger.cpp)

target_link_libraries(InHouseLogger rt)

# Use C++11 to compile our pass (i.e., supply -std=c++11).
target_compile_features(InHouseLogger PRIVATE cxx_range_for cxx_auto_type)

# LLVM is (typically) built with no C++ RTTI. We need to match that;
# otherwise, we'll get linker errors about missing RTTI data.
set_target_properties(InHouseLogger PROPERTIES
        COMPILE_FLAGS "-O2 -fno-rtti -fPIC")