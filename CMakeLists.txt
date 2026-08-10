cmake_minimum_required(VERSION 3.26)
project(StarryEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(BASE_OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin)

set(SHADERS_DIR   ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders)
set(FONTS_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts)
set(MODELS_DIR    ${CMAKE_CURRENT_SOURCE_DIR}/assets/models)
set(TEXTURES_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/assets/textures)
set(ICONS_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Dependencies.cmake)

add_subdirectory(src)

# add_subdirectory(external/pybind11)
# add_subdirectory(python)
