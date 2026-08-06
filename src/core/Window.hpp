#pragma once
#include"base.hpp"

namespace StarryEngine {
    class Window {
    public:
        struct Config {
            int posX = -1;
            int posY = -1;

            uint32_t width = 800;
            uint32_t height = 600;

            const char* title = "Vulkan App";
            bool resizable = false;
            int monitorIndex = 0;
            bool fullScreen = false;
            bool highDPI = false;
            bool scaleToMonitor = true;
            const char* iconPath = nullptr;

            // 透明窗口（桌面角色需要）：窗口支持逐像素 alpha 合成
            bool transparent = false;
            bool decorated = true;    // 有边框（透明窗口通常设为 false）
            bool floating = false;    // 置顶显示

            // 鼠标点击穿透：透明区域不拦截点击，事件落到桌面/下层窗口。
            // X11/Windows 走 GLFW_MOUSE_PASSTHROUGH；原生 Wayland 手动设空输入区。
            bool clickThrough = false;

            // Wayland 会话下默认强制 X11 (XWayland)，因其 Vulkan surface 不支持 alpha 合成。
            // 透明窗口需改走原生 Wayland（无边框时不受当初的装饰问题影响）。
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

        // 移动窗口位置（X11/Windows 生效；原生 Wayland 由 compositor 决定，会被忽略）
        void setPosition(int x, int y) { glfwSetWindowPos(mWindow, x, y); }

        // 运行时切换鼠标穿透（透明区不挡点击）。返回是否成功生效。
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