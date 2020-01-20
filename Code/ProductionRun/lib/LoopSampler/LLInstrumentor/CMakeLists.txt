add_library(LLInstrumentPass MODULE
        # List your source files here.
        LLInstrumentor.cpp
        BBProfiling.cpp
       )

target_link_libraries(LLInstrumentPass CommonLib)

# Use C++11 to compile our pass (i.e., supply -std=c++11).
target_compile_features(LLInstrumentPass PRIVATE cxx_range_for cxx_auto_type)

# LLVM is (typically) built with no C++ RTTI. We need to match that;
# otherwise, we'll get linker errors about missing RTTI data.
set_target_properties(LLInstrumentPass PROPERTIES
        COMPILE_FLAGS "-fno-rtti"
        )
