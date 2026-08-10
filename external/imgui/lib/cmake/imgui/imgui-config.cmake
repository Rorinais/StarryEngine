get_filename_component(_SE_IMGUI_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

add_library(imgui STATIC
    ${_SE_IMGUI_ROOT}/src/imgui.cpp
    ${_SE_IMGUI_ROOT}/src/imgui_draw.cpp
    ${_SE_IMGUI_ROOT}/src/imgui_tables.cpp
    ${_SE_IMGUI_ROOT}/src/imgui_widgets.cpp
    ${_SE_IMGUI_ROOT}/src/imgui_demo.cpp
    ${_SE_IMGUI_ROOT}/src/ImGuiFileDialog.cpp
    ${_SE_IMGUI_ROOT}/src/TextEditor.cpp
    ${_SE_IMGUI_ROOT}/src/backends/imgui_impl_glfw.cpp
    ${_SE_IMGUI_ROOT}/src/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui PUBLIC
    ${_SE_IMGUI_ROOT}/include
    ${_SE_IMGUI_ROOT}/include/backends
)
target_link_libraries(imgui PRIVATE Vulkan::Vulkan glfw)
