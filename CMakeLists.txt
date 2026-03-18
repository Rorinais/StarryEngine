cmake_minimum_required(VERSION 3.26)
project(StarryEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BASE_OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin)

set(SHADERS_DIR   ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders)
set(FONTS_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts)
set(MODELS_DIR    ${CMAKE_CURRENT_SOURCE_DIR}/assets/models)
set(TEXTURES_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/assets/textures)
set(ICONS_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons)

add_subdirectory(external/vulkan)
add_subdirectory(external/glfw)
add_subdirectory(external/assimp)
add_subdirectory(external/glm)
add_subdirectory(external/imgui)
add_subdirectory(external/spdlog)
add_subdirectory(src) 

#add_subdirectory(external/pybind11)
#add_subdirectory(python)


