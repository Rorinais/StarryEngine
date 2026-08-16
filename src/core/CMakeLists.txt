add_library(core STATIC
    src/FrameMonitor.cpp
    src/Window.cpp
    src/JobSystem.cpp
)

target_include_directories(core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(WAYLAND_CLIENT QUIET IMPORTED_TARGET wayland-client)
endif()
if(TARGET PkgConfig::WAYLAND_CLIENT)
    target_compile_definitions(core PRIVATE STARRY_HAVE_WAYLAND=1)
    target_link_libraries(core PRIVATE PkgConfig::WAYLAND_CLIENT)
    message(STATUS "[core] Wayland 可用：启用原生 Wayland 点击穿透")
else()
    message(STATUS "[core] 未找到 wayland-client，Wayland 点击穿透不可用（降级）")
endif()

target_link_libraries(core
PUBLIC
    renderer         
    glfw
    Vulkan::Vulkan
    glm::glm
    assimp::assimp
    imgui
    shaderc
PRIVATE
    event             
    stb_image    
)
