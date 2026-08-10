add_library(shaderc SHARED IMPORTED GLOBAL)
set_target_properties(shaderc PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/../../../include"
)
if(WIN32)
    set_target_properties(shaderc PROPERTIES
        MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
        MAP_IMPORTED_CONFIG_MINSIZEREL Release
        IMPORTED_IMPLIB_RELEASE "${CMAKE_CURRENT_LIST_DIR}/../../../lib/shaderc/shaderc_shared.lib"
        IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_LIST_DIR}/../../../lib/shaderc/shaderc_shared.dll"
        IMPORTED_IMPLIB_DEBUG "${CMAKE_CURRENT_LIST_DIR}/../../../lib/shaderc/shaderc_sharedd.lib"
        IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_LIST_DIR}/../../../lib/shaderc/shaderc_sharedd.dll"
    )
else()
    set_target_properties(shaderc PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/../../../lib/shaderc/libshaderc_shared.so.1"
    )
endif()
