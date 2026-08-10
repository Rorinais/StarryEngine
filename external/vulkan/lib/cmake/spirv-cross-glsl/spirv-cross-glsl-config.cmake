add_library(spirv-cross-glsl STATIC IMPORTED GLOBAL)
set_target_properties(spirv-cross-glsl PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/../../../include"
    INTERFACE_LINK_LIBRARIES spirv-cross-core
)
if(WIN32)
    set_target_properties(spirv-cross-glsl PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/../../../lib/spirv_cross/spirv-cross-glsl.lib"
    )
else()
    set_target_properties(spirv-cross-glsl PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/../../../lib/spirv_cross/libspirv-cross-glsl.a"
    )
endif()
