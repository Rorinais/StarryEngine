get_filename_component(VULKAN_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib" ABSOLUTE)
get_filename_component(VULKAN_INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include" ABSOLUTE)

add_library(vulkan SHARED IMPORTED GLOBAL)
add_library(shaderc SHARED IMPORTED GLOBAL)    


if(WIN32)
    set_target_properties(vulkan PROPERTIES
        IMPORTED_IMPLIB "${VULKAN_LIB_DIR}/vulkan-1.lib" 
        IMPORTED_LOCATION "${VULKAN_LIB_DIR}/vulkan-1.dll" 
        INTERFACE_INCLUDE_DIRECTORIES "${VULKAN_INC_DIR}"
    )

    set_target_properties(shaderc PROPERTIES
        IMPORTED_IMPLIB "${VULKAN_LIB_DIR}/shaderc/shaderc_sharedd.lib"
        IMPORTED_LOCATION "${VULKAN_LIB_DIR}/shaderc/shaderc_sharedd.dll"
        INTERFACE_INCLUDE_DIRECTORIES "${VULKAN_INC_DIR}"
    )
endif()

