idf_component_register(
    SRC_DIRS "src"
    INCLUDE_DIRS "src/include"
    REQUIRES driver freertos esp_timer json nvs_flash
)

if (CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-field-initializers -Wno-switch -Wno-deprecated-enum-enum-conversion")
endif()

set(ESP_SUPPORT_TOOL_DIR  "${CMAKE_CURRENT_LIST_DIR}/cmake"
    CACHE INTERNAL "Path to esp‑support helper cmake files")
