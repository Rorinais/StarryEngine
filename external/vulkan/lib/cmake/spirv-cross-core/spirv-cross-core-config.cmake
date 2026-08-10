add_library(spirv-cross-core STATIC IMPORTED GLOBAL)
set_target_properties(spirv-cross-core PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/../../../include"
)
if(WIN32)
    set_target_properties(spirv-cross-core PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/../../../lib/spirv_cross/spirv-cross-core.lib"
    )
else()
    set_target_properties(spirv-cross-core PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/../../../lib/spirv_cross/libspirv-cross-core.a"
    )
endif()
