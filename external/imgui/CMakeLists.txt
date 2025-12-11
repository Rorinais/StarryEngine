file(GLOB IMGUI_SOURCES 
    "*.cpp" 
    "backends/imgui_impl_glfw.cpp"  
    "backends/imgui_impl_vulkan.cpp"
)

add_library(imgui STATIC ${IMGUI_SOURCES})

target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/backends
)
target_link_libraries(imgui PRIVATE
    vulkan
    glfw
)
