idf_component_register(

    # Source files
    SRCS
        "${COMPONENT_DIR}/src/cloud_cb.c"
        "${COMPONENT_DIR}/src/include"
        "${COMPONENT_DIR}/src/known_registers.c"
        "${COMPONENT_DIR}/src/mb_rtu.c"
        "${COMPONENT_DIR}/src/nvs_fw_cfg.c"
        "${COMPONENT_DIR}/src/str_utils.c"
        "${COMPONENT_DIR}/src/trackle-gateway-master-modbus.c"

    # Public interface headers
    INCLUDE_DIRS
        "${COMPONENT_DIR}/include"

    # Private headers
    PRIV_INCLUDE_DIRS
        "${COMPONENT_DIR}/src/include"

    # Components required by units that have to be built inside this component
    REQUIRES
        trackle-library-esp-idf
        trackle-modbus-esp-idf
        
)
