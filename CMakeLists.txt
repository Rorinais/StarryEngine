cmake_minimum_required(VERSION 3.15)
project(StarryEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置基础输出目录
set(BASE_OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin)

# 资源路径配置
set(SHADERS_DIR   ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders)
set(FONTS_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts)
set(MODELS_DIR    ${CMAKE_CURRENT_SOURCE_DIR}/assets/models)
set(TEXTURES_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/assets/textures)
set(ICONS_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons)

# 添加子目录
add_subdirectory(external/vulkan)
add_subdirectory(external/glfw)
add_subdirectory(external/assimp)
add_subdirectory(external/glm)
add_subdirectory(external/imgui)
add_subdirectory(external/spdlog)
#add_subdirectory(external/opencv) //还不需要opencv
 
add_subdirectory(src) 

#DEBUG 
#cmake  -DCMAKE_BUILD_TYPE=Debug ..

#Relese
#cmake  -DCMAKE_BUILD_TYPE=Release ..



