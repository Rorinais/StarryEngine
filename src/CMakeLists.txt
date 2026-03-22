add_library(BaseInterface INTERFACE)
target_include_directories(BaseInterface INTERFACE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/external
)

target_link_libraries(BaseInterface INTERFACE
    assimp
    glfw
    glm
    imgui
    spdlog
    vulkan 
    shaderc
    spirv-cross-glsl
    spirv-cross-core
)

# 包含辅助函数
include(../cmake/EngineHelpers.cmake)

# 设置全局资源目录（供辅助函数使用）
set(SHADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets/shaders")
set(FONTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets/fonts")
set(MODELS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets/models")
set(TEXTURES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets/textures")
set(ICONS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets/icons")

# 添加子目录（库）
add_subdirectory(core)
add_subdirectory(assets)
add_subdirectory(scene)
add_subdirectory(event)
add_subdirectory(utils)
add_subdirectory(logging)
add_subdirectory(renderer)
add_subdirectory(application)

# 使用封装函数创建可执行文件
add_engine_executable(
    TARGET_NAME ForwardRender_demo
    SOURCES demo/ForwardRender_demo.cpp
    OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin
    SHADERS_DIR ${SHADERS_DIR}
    FONTS_DIR ${FONTS_DIR}
    MODELS_DIR ${MODELS_DIR}
    TEXTURES_DIR ${TEXTURES_DIR}
    ICONS_DIR ${ICONS_DIR}
)

add_engine_executable(
    TARGET_NAME DeferredRender_demo
    SOURCES demo/DeferredRender_demo.cpp
    OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin
    SHADERS_DIR ${SHADERS_DIR}
    FONTS_DIR ${FONTS_DIR}
    MODELS_DIR ${MODELS_DIR}
    TEXTURES_DIR ${TEXTURES_DIR}
    ICONS_DIR ${ICONS_DIR}
)