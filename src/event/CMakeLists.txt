add_library(event STATIC
    src/EventDispatcher.cpp
)

target_include_directories(event PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(event
PUBLIC
    renderer          # Events.hpp 引用 renderer 类型
)
