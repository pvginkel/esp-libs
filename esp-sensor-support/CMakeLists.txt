idf_component_register(
    SRC_DIRS "src" "src/sensirion_gas_index_algorithm"
    INCLUDE_DIRS "src/include"
    PRIV_INCLUDE_DIRS "src/sensirion_gas_index_algorithm"
    REQUIRES esp_adc driver
)

# The vendored gas index algorithm, Sensirion/gas-index-algorithm at 2ef9f13 (3.2.2 and later), is
# C, and builds without -Wconversion warnings as it is.
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-missing-field-initializers
    -Wno-switch
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-enum-enum-conversion>
    -Wconversion
)
