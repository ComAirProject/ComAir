add_library(NoInstrumentPass MODULE
        # List your source files here.
        NoInstrumentor.cpp
        BBProfiling.cpp
       )

target_link_libraries(NoInstrumentPass CommonLib)

# Use C++11 to compile our pass (i.e., supply -std=c++11).
target_compile_features(NoInstrumentPass PRIVATE cxx_range_for cxx_auto_type)

# NoVM is (typicaNoy) built with no C++ RTTI. We need to match that;
# otherwise, we'No get linker errors about missing RTTI data.
set_target_properties(NoInstrumentPass PROPERTIES
        COMPILE_FLAGS "-fno-rtti"
        )
