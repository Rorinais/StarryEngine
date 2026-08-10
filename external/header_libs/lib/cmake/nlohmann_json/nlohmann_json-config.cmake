add_library(nlohmann_json INTERFACE IMPORTED GLOBAL)
set_target_properties(nlohmann_json PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/../../../include"
)
