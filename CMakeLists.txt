idf_component_register(

    # Source files
    SRCS
        "${COMPONENT_DIR}/src/*.c"

    # Public interface headers
    INCLUDE_DIRS
        "${COMPONENT_DIR}/include"

    # Private headers
    PRIV_INCLUDE_DIRS
        "${COMPONENT_DIR}/src/include"
        
)
