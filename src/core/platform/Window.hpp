#pragma once
#include"../../base.hpp"

struct GLFWimage;

namespace StarryEngine {
    // 前向声明GLFWimage


    class Window {
    public:
        struct Config {
            uint32_t width = 800;
            uint32_t height = 600;
            const char* title = "Vulkan App";
            bool resizable = false;
            int monitorIndex = 0;
            bool fullScreen = false;
            bool highDPI = false;
            const char* iconPath = nullptr; // 添加图标路径配置
        };

        using Ptr = std::shared_ptr<Window>;
        static Ptr create(const Window::Config& config) { return std::make_shared<Window>(config); }

        using ResizeCallback = std::function<void(int, int)>;
        using KeyCallback = std::function<void(int key, int action)>;

        Window(const Window::Config& config);
        ~Window();

        GLFWwindow* getHandle() const noexcept { return mWindow; }

        void setResizeCallback(ResizeCallback callback);
        void setKeyCallback(KeyCallback callback);

        // 添加图标设置方法
        bool setIcon(const char* imagePath);
        bool setIconFromMemory(const unsigned char* imageData, int width, int height, int channels = 4);

        bool shouldClose() const;
        void pollEvents() const;

        float getAspectRatio() const noexcept {
            return static_cast<float>(mConfig.width) / static_cast<float>(mConfig.height);
        }

    private:
        static void terminateGLFW();
        bool loadIconFromFile(const char* path, GLFWimage& image);
        bool loadIconFromMemory(const unsigned char* data, int width, int height, int channels, GLFWimage& image);

    private:
        GLFWwindow* mWindow = nullptr;
        Config mConfig;

        ResizeCallback mResizeCallback;
        KeyCallback mKeyCallback;

        // 存储图标数据，确保在窗口生命周期内有效
        std::vector<unsigned char> mIconData;
    };

}