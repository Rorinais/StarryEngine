add_library(utils STATIC
    src/FileUtils.cpp
)

target_include_directories(utils PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(utils
PUBLIC
    renderer          # FileUtils 反射 shader 用 renderer 类型（头外泄）
    shaderc
)
