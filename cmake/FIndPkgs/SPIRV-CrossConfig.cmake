# SPIRV-CrossConfig.cmake

set(SPIRV-Cross_FOUND TRUE)

# Header location
#set(SPIRV-Cross_INCLUDE_DIR "/opt/local/include")

# Library location
find_library(SPIRV_CROSS_CORE_LIBRARY NAMES spirv-cross-core PATHS "/usr/local/lib/" REQUIRED)
find_library(SPIRV_CROSS_GLSL_LIBRARY NAMES spirv-cross-glsl PATHS "/usr/local/lib/" REQUIRED)
find_library(SPIRV_CROSS_HLSL_LIBRARY NAMES spirv-cross-hlsl PATHS "/usr/local/lib/" REQUIRED)
find_library(SPIRV_CROSS_MSL_LIBRARY NAMES spirv-cross-msl PATHS "/usr/local/lib/" REQUIRED)


# Define targets
add_library(SPIRV-Cross::spirv-cross-core UNKNOWN IMPORTED)
set_target_properties(SPIRV-Cross::spirv-cross-core PROPERTIES
        IMPORTED_LOCATION "${SPIRV_CROSS_CORE_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SPIRV-Cross_INCLUDE_DIR}"
)

add_library(SPIRV-Cross::spirv-cross-glsl UNKNOWN IMPORTED)
set_target_properties(SPIRV-Cross::spirv-cross-glsl PROPERTIES
        IMPORTED_LOCATION "${SPIRV_CROSS_GLSL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SPIRV-Cross_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES SPIRV-Cross::spirv-cross-core
)

add_library(SPIRV-Cross::spirv-cross-msl UNKNOWN IMPORTED)
set_target_properties(SPIRV-Cross::spirv-cross-msl PROPERTIES
        IMPORTED_LOCATION "${SPIRV_CROSS_MSL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SPIRV-Cross_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES SPIRV-Cross::spirv-cross-core
)

add_library(SPIRV-Cross::spirv-cross-hlsl UNKNOWN IMPORTED)
set_target_properties(SPIRV-Cross::spirv-cross-hlsl PROPERTIES
        IMPORTED_LOCATION "${SPIRV_CROSS_HLSL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SPIRV-Cross_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES SPIRV-Cross::spirv-cross-core
)