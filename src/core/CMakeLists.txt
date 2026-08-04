
file(GLOB SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(core STATIC ${SRC})

# Wayland 点击穿透：探测 wayland-client（用于 wl_surface_set_input_region 设空输入区）。
# 没有则优雅降级——Wayland 下无法穿透，X11/Windows 走 GLFW_MOUSE_PASSTHROUGH 不受影响。
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
PRIVATE
    BaseInterface
)
