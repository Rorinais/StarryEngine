if(NOT WIN32)
    set(assimp_FOUND FALSE)
    return()
endif()

get_filename_component(_SE_ASSIMP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

add_library(assimp::assimp SHARED IMPORTED GLOBAL)
set_target_properties(assimp::assimp PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${_SE_ASSIMP_ROOT}/include"
)
if(MSVC)
    set_target_properties(assimp::assimp PROPERTIES
        IMPORTED_IMPLIB "${_SE_ASSIMP_ROOT}/lib/assimp-vc143-mt.lib"
        IMPORTED_LOCATION "${_SE_ASSIMP_ROOT}/lib/assimp-vc143-mt.dll"
    )
elseif(MINGW)
    set_target_properties(assimp::assimp PROPERTIES
        IMPORTED_IMPLIB "${_SE_ASSIMP_ROOT}/lib/libassimp.dll.a"
        IMPORTED_LOCATION "${_SE_ASSIMP_ROOT}/lib/libassimp-6.dll"
    )
endif()
