idf_component_register(
    SRC_DIRS "src"
    INCLUDE_DIRS "src/include"
    REQUIRES driver freertos esp_timer json nvs_flash
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-missing-field-initializers
    -Wno-switch
    -Wno-deprecated-enum-enum-conversion
    -Wconversion
)

set(ESP_SUPPORT_TOOL_DIR  "${CMAKE_CURRENT_LIST_DIR}/cmake"
    CACHE INTERNAL "Path to esp-support helper cmake files")
