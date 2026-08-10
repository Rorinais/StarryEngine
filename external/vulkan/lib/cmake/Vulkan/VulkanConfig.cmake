add_library(Vulkan::Vulkan SHARED IMPORTED GLOBAL)
set_target_properties(Vulkan::Vulkan PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/../../../include"
)
if(WIN32)
    set_target_properties(Vulkan::Vulkan PROPERTIES
        IMPORTED_IMPLIB "${CMAKE_CURRENT_LIST_DIR}/../../vulkan-1.lib"
        IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/../../vulkan-1.dll"
    )
else()
    set_target_properties(Vulkan::Vulkan PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/../../libvulkan.so.1.4.304"
    )
endif()
