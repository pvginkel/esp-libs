# src/core/ is the pure core: it includes the C++ standard library only, so test/ builds it natively.
idf_component_register(
    SRC_DIRS "src/core"
    INCLUDE_DIRS "src/core"
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-missing-field-initializers
    -Wno-switch
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-enum-enum-conversion>
    -Wconversion
)
