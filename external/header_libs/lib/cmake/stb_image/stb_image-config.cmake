add_library(stb_image INTERFACE IMPORTED GLOBAL)
set_target_properties(stb_image PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/../../../include"
)
