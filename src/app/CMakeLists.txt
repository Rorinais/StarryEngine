# L3 应用层：把"配置"变成"一次运行"。不包含内容，也不包含渲染实现。
add_library(app STATIC
    src/AppConfig.cpp
    src/Diagnostics.cpp
)

target_include_directories(app PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(app
PUBLIC
    application
PRIVATE
    logging renderer
)
