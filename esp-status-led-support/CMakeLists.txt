idf_component_register(
    SRC_DIRS "src"
    INCLUDE_DIRS "src/include"
    REQUIRES led_strip
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-missing-field-initializers
    -Wno-switch
    -Wno-deprecated-enum-enum-conversion
    -Wconversion
)
