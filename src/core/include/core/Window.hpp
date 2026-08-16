#pragma once
#include <core/base.hpp>

namespace StarryEngine {
    class Window {
    public:
        struct Config {
            int posX = -1;
            int posY = -1;

            uint32_t width = 800;
            uint32_t height = 600;
            int monitorIndex = 0;

            const char* title = "Vulkan App";
            const char* iconPath = nullptr;

            bool resizable = false;
            bool fullScreen = false;
            bool highDPI = false;
            bool scaleToMonitor = true;
            bool transparent = false;
            bool decorated = true;    
            bool floating = false;    
            bool clickThrough = false;
            bool nativeWayland = false;
        };

        using Ptr = std::shared_ptr<Window>;
        static Ptr create(const Window::Config& config) { return std::make_shared<Window>(config); }

        using ResizeCallback = std::function<void(int, int)>;
        using KeyCallback = std::function<void(int key, int action)>;

        Window(const Window::Config& config);
        ~Window();

        GLFWwindow* getHandle() const noexcept { return mWindow; }
		uint16_t getWidth() const noexcept { return mConfig.width; }
		uint16_t getHeight() const noexcept { return mConfig.height; }

        bool setIcon(const char* imagePath);
        bool setIconFromMemory(const unsigned char* imageData, int width, int height, int channels = 4);

        bool shouldClose() const;
        void pollEvents() const;

        void setPosition(int x, int y) { glfwSetWindowPos(mWindow, x, y); }

        bool setClickThrough(bool enable);

        float getAspectRatio() const noexcept {
            return static_cast<float>(mConfig.width) / static_cast<float>(mConfig.height);
        }

    private:
        static void terminateGLFW();
        bool loadIconFromFile(const char* path, GLFWimage& image);
        bool loadIconFromMemory(const unsigned char* data, int width, int height, int channels, GLFWimage& image);
        bool applyWaylandClickThrough(bool enable);
        void reapplyClickThrough();

    private:
        GLFWwindow* mWindow = nullptr;
        Config mConfig;

        std::vector<unsigned char> mIconData;
    };
}