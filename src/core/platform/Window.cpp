#include "Window.hpp"
#include <stb_image.h>
#include <vector>

namespace StarryEngine {
        static void glfwErrorCallback(int error, const char* description) {
            std::cerr << "GLFW Error (" << error << "): " << description << std::endl;
        }

        void Window::terminateGLFW() {
            glfwTerminate();
            glfwSetErrorCallback(nullptr);
        }

        Window::Window(const Config& config) : mConfig(config) {
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
            int width = mConfig.width;
            int height = mConfig.height;

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

            // 如果配置中指定了图标路径，设置图标
            if (mConfig.iconPath) {

                //td::cout << "Setting window icon from file: " << config.iconPath << std::endl;
                setIcon(mConfig.iconPath);
            }

            glfwSetWindowUserPointer(mWindow, this);
            glfwSetWindowSizeCallback(mWindow, [](GLFWwindow* window, int width, int height) {
                auto* manager = static_cast<Window*>(glfwGetWindowUserPointer(window));
                if (manager && manager->mResizeCallback) {
                    manager->mResizeCallback(width, height);
                    glfwPostEmptyEvent();
                }
                });

            glfwSetKeyCallback(mWindow, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
                auto* manager = static_cast<Window*>(glfwGetWindowUserPointer(window));
                if (manager && manager->mKeyCallback) {
                    manager->mKeyCallback(key, action);
                }
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
                // 如果数据不是RGBA格式，需要转换
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
                // 直接使用RGBA数据
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

        void Window::setResizeCallback(ResizeCallback callback) {
            mResizeCallback = callback;
        }

        void Window::setKeyCallback(KeyCallback callback) {
            mKeyCallback = callback;
        }

        bool Window::shouldClose() const {
            return glfwWindowShouldClose(mWindow);
        }

        void Window::pollEvents() const {
            glfwPollEvents();
        }
}