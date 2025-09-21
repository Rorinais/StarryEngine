get_filename_component(GLFW_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)

add_library(glfw SHARED IMPORTED GLOBAL)  

if(WIN32)
    if(MSVC)
        set_target_properties(glfw PROPERTIES
            IMPORTED_IMPLIB "${GLFW_LIB_DIR}/glfw3_mt.lib"
            IMPORTED_LOCATION "${GLFW_LIB_DIR}/glfw3.dll"
            INTERFACE_INCLUDE_DIRECTORIES "${GLFW_LIB_DIR}"
        )

    elseif(MINGW)
        set_target_properties(glfw PROPERTIES
            IMPORTED_IMPLIB "${GLFW_LIB_DIR}/libglfw3.a"
            IMPORTED_LOCATION "${GLFW_LIB_DIR}/libglfw3.dll"
            INTERFACE_INCLUDE_DIRECTORIES "${GLFW_LIB_DIR}"
        )
    endif()
endif()








