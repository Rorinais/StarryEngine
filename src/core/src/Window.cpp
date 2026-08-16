#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <cstdlib>
#include <algorithm>
#include <core/Window.hpp>
#include <event/Events.hpp>

#if defined(__linux__) && defined(STARRY_HAVE_WAYLAND)
#include <wayland-client.h>
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3native.h>
#endif

namespace StarryEngine {

#if defined(__linux__) && defined(STARRY_HAVE_WAYLAND)
namespace {
    struct wl_compositor* g_wlCompositor = nullptr;

    const struct wl_registry_listener g_wlRegistryListener = {
        [](void*, struct wl_registry* registry, uint32_t name,
           const char* interface, uint32_t version) {
            if (std::string(interface) == wl_compositor_interface.name) {
                g_wlCompositor = static_cast<struct wl_compositor*>(
                    wl_registry_bind(registry, name, &wl_compositor_interface,
                                     std::min(version, 4u)));
            }
        },
        [](void*, struct wl_registry*, uint32_t) {}
    };

    struct wl_compositor* getWlCompositor(struct wl_display* dpy) {
        if (g_wlCompositor) return g_wlCompositor;
        struct wl_registry* registry = wl_display_get_registry(dpy);
        wl_registry_add_listener(registry, &g_wlRegistryListener, nullptr);
        wl_display_roundtrip(dpy);
        wl_registry_destroy(registry);
        return g_wlCompositor;
    }
}
#endif

        static void glfwErrorCallback(int error, const char* description) {
            std::cerr << "GLFW Error (" << error << "): " << description << std::endl;
        }

        constexpr int kClickThroughHandleHeight = 40;

        void Window::terminateGLFW() {
            glfwTerminate();
            glfwSetErrorCallback(nullptr);
        }

        Window::Window(const Config& config) : mConfig(config) {
#ifdef __linux__
            const char* sessionType = std::getenv("XDG_SESSION_TYPE");
            if (sessionType && std::string(sessionType) == "wayland") {
                glfwInitHint(GLFW_PLATFORM, mConfig.nativeWayland ? GLFW_PLATFORM_WAYLAND : GLFW_PLATFORM_X11);
            }
#endif

            if (!glfwInit()) {
                throw std::runtime_error("Failed to initialize GLFW");
            }
            glfwSetErrorCallback(glfwErrorCallback);

            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, mConfig.resizable ? GLFW_TRUE : GLFW_FALSE);
            if (mConfig.highDPI) {
                glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
            }

            if (mConfig.transparent) {
                glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
            }
            glfwWindowHint(GLFW_DECORATED, mConfig.decorated ? GLFW_TRUE : GLFW_FALSE);
            if (mConfig.floating) {
                glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
            }

            GLFWmonitor* monitor = nullptr;
            const GLFWvidmode* mode = nullptr;
            int width = static_cast<int>(mConfig.width);
            int height = static_cast<int>(mConfig.height);

            if (mConfig.scaleToMonitor && !mConfig.fullScreen) {
                GLFWmonitor* primary = glfwGetPrimaryMonitor();
                if (primary) {
                    float xscale = 1.0f, yscale = 1.0f;
                    glfwGetMonitorContentScale(primary, &xscale, &yscale);
                    width = static_cast<int>(width * xscale);
                    height = static_cast<int>(height * yscale);
                }
            }

            if (mConfig.fullScreen) {
                int monitorCount;
                GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

                if (monitorCount == 0) {
                    terminateGLFW();
                    throw std::runtime_error("No monitors found");
                }

                if (mConfig.monitorIndex >= 0 && mConfig.monitorIndex < monitorCount) {
                    monitor = monitors[mConfig.monitorIndex];
                    mode = glfwGetVideoMode(monitor);
                    width = mode->width;
                    height = mode->height;
                }
                else {
                    terminateGLFW();
                    throw std::runtime_error("Invalid monitor index: " + std::to_string(mConfig.monitorIndex));
                }
            }

            if (mConfig.posX >= 0 && mConfig.posY >= 0){
                glfwWindowHint(GLFW_POSITION_X,mConfig.posX);
                glfwWindowHint(GLFW_POSITION_Y,mConfig.posY);
            }

            mWindow = glfwCreateWindow(width, height, mConfig.title, monitor, nullptr);

            if (!mWindow) {
                terminateGLFW();
                throw std::runtime_error("Failed to create GLFW window");
            }

            if (mConfig.posX >= 0 && mConfig.posY >= 0) {
                glfwSetWindowPos(mWindow, mConfig.posX, mConfig.posY);
            }

            if (mConfig.clickThrough) {
                if (!setClickThrough(true)) {
                    std::cerr << "[Window] 点击穿透在当前平台不可用" << std::endl;
                }
            }

            if (mConfig.iconPath) {
                setIcon(mConfig.iconPath);
            }

            glfwSetWindowUserPointer(mWindow, this);
            glfwSetWindowSizeCallback(mWindow, [](GLFWwindow* window, int width, int height) {
                GetEventDispatcher().dispatch<WindowResizeEvent>(width, height);
                auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
                if (self) self->reapplyClickThrough();
                });

            glfwSetKeyCallback(mWindow, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
                GetEventDispatcher().dispatch<KeyEvent>(key, scancode, action, mods);
                });

            glfwSetMouseButtonCallback(mWindow, [](GLFWwindow* window, int button, int action, int mods) {
                GetEventDispatcher().dispatch<MouseButtonEvent>(button, action, mods);
                });

            glfwSetCursorPosCallback(mWindow, [](GLFWwindow* window, double x, double y) {
                GetEventDispatcher().dispatch<MouseMoveEvent>(x, y);
                });

            glfwSetScrollCallback(mWindow, [](GLFWwindow* window, double xOffset, double yOffset) {
                GetEventDispatcher().dispatch<MouseScrollEvent>(xOffset, yOffset);
                });

        }

        Window::~Window() {
            if (mWindow) {
                glfwDestroyWindow(mWindow);
            }
            terminateGLFW();
        }

        bool Window::loadIconFromFile(const char* path, GLFWimage& image) {
            int width, height, channels;
            unsigned char* data = stbi_load(path, &width, &height, &channels, 4); // 强制RGBA
            if (!data) {
                std::cerr << "Failed to load icon: " << path << std::endl;
                return false;
            }

            mIconData.assign(data, data + width * height * 4);
            stbi_image_free(data);

            image.width = width;
            image.height = height;
            image.pixels = mIconData.data();

            return true;
        }

        bool Window::loadIconFromMemory(const unsigned char* data, int width, int height, int channels, GLFWimage& image) {
            if (channels != 4) {
                std::vector<unsigned char> rgbaData(width * height * 4);

                if (channels == 1) {
                    // 灰度转RGBA
                    for (int i = 0; i < width * height; ++i) {
                        rgbaData[i * 4] = data[i];
                        rgbaData[i * 4 + 1] = data[i];
                        rgbaData[i * 4 + 2] = data[i];
                        rgbaData[i * 4 + 3] = 255;
                    }
                }
                else if (channels == 3) {
                    for (int i = 0; i < width * height; ++i) {
                        rgbaData[i * 4] = data[i * 3];
                        rgbaData[i * 4 + 1] = data[i * 3 + 1];
                        rgbaData[i * 4 + 2] = data[i * 3 + 2];
                        rgbaData[i * 4 + 3] = 255;
                    }
                }
                else {
                    std::cerr << "Unsupported number of channels: " << channels << std::endl;
                    return false;
                }

                mIconData = std::move(rgbaData);
            }
            else {
                mIconData.assign(data, data + width * height * 4);
            }

            image.width = width;
            image.height = height;
            image.pixels = mIconData.data();

            return true;
        }

        bool Window::setIcon(const char* imagePath) {
            GLFWimage image;
            if (!loadIconFromFile(imagePath, image)) {
                return false;
            }

            glfwSetWindowIcon(mWindow, 1, &image);
            return true;
        }

        bool Window::setIconFromMemory(const unsigned char* imageData, int width, int height, int channels) {
            GLFWimage image;
            if (!loadIconFromMemory(imageData, width, height, channels, image)) {
                return false;
            }

            glfwSetWindowIcon(mWindow, 1, &image);
            return true;
        }

        bool Window::shouldClose() const {
            return glfwWindowShouldClose(mWindow);
        }

        void Window::pollEvents() const {
            glfwPollEvents();
        }

        bool Window::applyWaylandClickThrough(bool enable) {
#if defined(__linux__) && defined(STARRY_HAVE_WAYLAND)
            struct wl_display* dpy = glfwGetWaylandDisplay();
            struct wl_surface* surface = glfwGetWaylandWindow(mWindow);
            if (!dpy || !surface) return false;

            struct wl_compositor* compositor = getWlCompositor(dpy);
            if (!compositor) return false;

            if (enable) {
                int winW = 0, winH = 0;
                glfwGetWindowSize(mWindow, &winW, &winH);
                struct wl_region* region = wl_compositor_create_region(compositor);
                if (!region) return false;
                wl_region_add(region, 0, 0,
                              winW > 0 ? winW : 4096,
                              kClickThroughHandleHeight);
                wl_surface_set_input_region(surface, region);
                wl_region_destroy(region);
            } else {
                wl_surface_set_input_region(surface, nullptr);
            }
            return true;
#else
            (void)enable;
            return false;
#endif
        }

        bool Window::setClickThrough(bool enable) {
            mConfig.clickThrough = enable;
#if defined(__linux__) && defined(STARRY_HAVE_WAYLAND)
            if (glfwGetWaylandDisplay() && glfwGetWaylandWindow(mWindow)) {
                return applyWaylandClickThrough(enable);
            }
#endif
#ifdef __linux__
            if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
                std::cerr << "[Window] Wayland 下点击穿透需 wayland-client 支持（当前不可用）" << std::endl;
                return false;
            }
#endif
            glfwSetWindowAttrib(mWindow, GLFW_MOUSE_PASSTHROUGH, enable ? GLFW_TRUE : GLFW_FALSE);
            return true;
        }

        void Window::reapplyClickThrough() {
            if (!mConfig.clickThrough) return;
            setClickThrough(true);
        }
}