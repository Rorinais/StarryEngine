# 包含辅助函数
include(../cmake/EngineHelpers.cmake)

# 设置全局资源目录（供辅助函数使用）
set(SHADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets/shaders")
set(FONTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets/fonts")
set(MODELS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets/models")
set(TEXTURES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets/textures")
set(ICONS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets/icons")
set(CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets/configs")
set(MATERIAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets/materials")

# 添加子目录（库）
add_subdirectory(core)
add_subdirectory(assets)
add_subdirectory(scene)
add_subdirectory(event)
add_subdirectory(utils)
add_subdirectory(logging)
add_subdirectory(renderer)
add_subdirectory(application)
add_subdirectory(ui)


add_engine_executable(
    TARGET_NAME StarryEngine
    SOURCES ../demo/StarryEngine.cpp
    SHADERS_DIR ${SHADERS_DIR}
    FONTS_DIR ${FONTS_DIR}
    MODELS_DIR ${MODELS_DIR}
    TEXTURES_DIR ${TEXTURES_DIR}
    ICONS_DIR ${ICONS_DIR}
)

