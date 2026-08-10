add_library(logging STATIC
    src/Logger.cpp
)

target_include_directories(logging PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(logging
PUBLIC
    assets            # Logger 头引用 assets 类型
    renderer          # Logger 头引用 renderer 类型
    spdlog::spdlog
    glm::glm
)
