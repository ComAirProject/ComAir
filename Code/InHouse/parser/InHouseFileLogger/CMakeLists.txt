add_executable(InHouseFileLogger
        # List your source files here.
        InHouseFileLogger.cpp)

target_link_libraries(InHouseFileLogger rt)

# Use C++11 to compile our pass (i.e., supply -std=c++11).
target_compile_features(InHouseFileLogger PRIVATE cxx_range_for cxx_auto_type)

# LLVM is (typically) built with no C++ RTTI. We need to match that;
# otherwise, we'll get linker errors about missing RTTI data.
set_target_properties(InHouseFileLogger PROPERTIES
        COMPILE_FLAGS "-O2 -fno-rtti -fPIC")