# PUBLIC FUNCTION
#   raw_partition(
#       NAME        <partition-name>
#       GENERATOR   <script> [ARGS …]
#       SIZE        <bytes> | AUTO                # default=AUTO (from CSV)
#       DEPENDS     <list-of-extra-deps>
#   )
#
# It builds the binary, pads/truncates it, and wires it into:
#   - its own `${NAME}-flash` target
#   - the global `flash` target used by idf.py
#
cmake_policy(SET CMP0057 NEW)             # allow IN_LIST in if()

function(raw_partition)
    set(options)                          # none
    set(oneValueArgs NAME SIZE FILE)
    set(multiValueArgs DEPENDS)

    cmake_parse_arguments(RP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # --- sanity checks -----------------------------------------------------
    if(NOT RP_NAME)
        message(FATAL_ERROR "raw_partition(): NAME is mandatory")
    endif()
    if(NOT RP_FILE)
        message(FATAL_ERROR "raw_partition(): FILE is mandatory")
    endif()

    # --- partition size ----------------------------------------------------
    if(NOT RP_SIZE OR RP_SIZE STREQUAL "AUTO")
        partition_table_get_partition_info(RP_SIZE
            "--partition-name ${RP_NAME}" "size")
    endif()

    set(out_bin ${CMAKE_BINARY_DIR}/${RP_NAME}.bin)

    add_custom_command(OUTPUT ${out_bin}
        COMMAND ${PYTHON} ${ESP_SUPPORT_TOOL_DIR}/pad.py
        ${RP_FILE} ${RP_SIZE} ${out_bin}
        DEPENDS ${RP_FILE} ${RP_DEPENDS}
        COMMENT "Copy ${RP_FILE} to ${out_bin}"
    )

    # convenience ALL-target so IDEs build it automatically
    add_custom_target(${RP_NAME}_bin ALL DEPENDS ${out_bin})

    # --- flash wiring (same as before, just encapsulated) ------------------
    idf_component_get_property(ESPTOOL_ARGS esptool_py FLASH_ARGS)
    idf_component_get_property(ESPTOOL_SUB  esptool_py FLASH_SUB_ARGS)

    esptool_py_flash_target(${RP_NAME}-flash "${ESPTOOL_ARGS}" "${ESPTOOL_SUB}")
    esptool_py_flash_to_partition(${RP_NAME}-flash ${RP_NAME} ${out_bin})
    add_dependencies(${RP_NAME}-flash ${RP_NAME}_bin)

    # integrate into the standard ”idf.py flash”
    esptool_py_flash_to_partition(flash ${RP_NAME} ${out_bin})
    add_dependencies(flash ${RP_NAME}_bin)
endfunction()
