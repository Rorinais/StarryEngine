add_library(glfw SHARED IMPORTED GLOBAL)
set_target_properties(glfw PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/../../../include"
    INTERFACE_COMPILE_DEFINITIONS "GLFW_INCLUDE_NONE"
)
if(WIN32)
    if(MSVC)
        set_target_properties(glfw PROPERTIES
            IMPORTED_IMPLIB "${CMAKE_CURRENT_LIST_DIR}/../../../lib/glfw3_mt.lib"
            IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/../../../lib/glfw3.dll"
        )
    elseif(MINGW)
        set_target_properties(glfw PROPERTIES
            IMPORTED_IMPLIB "${CMAKE_CURRENT_LIST_DIR}/../../../lib/libglfw3dll.a"
            IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/../../../lib/glfw3.dll"
        )
    endif()
elseif(UNIX AND NOT APPLE)
    set_target_properties(glfw PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/../../../lib/libglfw.so.3.4"
    )
endif()
