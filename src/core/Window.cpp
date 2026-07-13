#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <cstdlib>
#include "Window.hpp"
#include"../event/Events.hpp"

namespace StarryEngine {
        static void glfwErrorCallback(int error, const char* description) {
            std::cerr << "GLFW Error (" << error << "): " << description << std::endl;
        }

        void Window::terminateGLFW() {
            glfwTerminate();
            glfwSetErrorCallback(nullptr);
        }

        Window::Window(const Config& config) : mConfig(config) {
#ifdef __linux__
            // Linux Wayland + GNOME 下原生 Wayland 后端窗口装饰有问题，改用 X11 (XWayland)
            const char* sessionType = std::getenv("XDG_SESSION_TYPE");
            if (sessionType && std::string(sessionType) == "wayland") {
                glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
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

            GLFWmonitor* monitor = nullptr;
            const GLFWvidmode* mode = nullptr;
            int width = static_cast<int>(mConfig.width);
            int height = static_cast<int>(mConfig.height);

            // 根据显示器缩放因子调整窗口像素尺寸，使逻辑尺寸匹配预期
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

            mWindow = glfwCreateWindow(width, height, mConfig.title, monitor, nullptr);

            if (!mWindow) {
                terminateGLFW();
                throw std::runtime_error("Failed to create GLFW window");
            }

            if (mConfig.iconPath) {
                setIcon(mConfig.iconPath);
            }

            glfwSetWindowUserPointer(mWindow, this);
            glfwSetWindowSizeCallback(mWindow, [](GLFWwindow* window, int width, int height) {
                GetEventDispatcher().dispatch<WindowResizeEvent>(width, height);
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

            // 存储数据以确保生命周期
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
                    // RGB转RGBA
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
}