# src/core/ is the pure core: it includes the C++ standard library only, so test/ builds it natively.
# src/ around it is the ESP-IDF side: the I2S output and the player.
idf_component_register(
    SRC_DIRS "src/core" "src"
    INCLUDE_DIRS "src/core" "src/include"
    REQUIRES esp_driver_i2s esp_driver_gpio
    PRIV_REQUIRES esp_timer heap
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-missing-field-initializers
    -Wno-switch
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-enum-enum-conversion>
    -Wconversion
)
