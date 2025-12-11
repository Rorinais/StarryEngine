get_filename_component(ASSIMP_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assimp/lib" ABSOLUTE)
get_filename_component(ASSIMP_INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)

add_library(assimp SHARED IMPORTED GLOBAL)  


if(WIN32)
    set_target_properties(assimp PROPERTIES
        IMPORTED_IMPLIB "${ASSIMP_LIB_DIR}/assimp-vc143-mt.lib"   
        IMPORTED_LOCATION "${ASSIMP_LIB_DIR}/assimp-vc143-mt.dll"
        INTERFACE_INCLUDE_DIRECTORIES "${ASSIMP_INC_DIR}"
    )
endif()

