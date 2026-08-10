add_library(vk_mem_alloc INTERFACE IMPORTED GLOBAL)
set_target_properties(vk_mem_alloc PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/../../../include"
)
