add_library(ui STATIC
    src/ImGuiExecutor.cpp
    src/ImGuiManager.cpp
)

target_include_directories(ui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(ui
PUBLIC
    renderer          # ImGuiExecutor.hpp 引用 renderer 类型
    imgui
PRIVATE
    core              # 仅 cpp 用（循环，LINK_GROUP 处理）
)
