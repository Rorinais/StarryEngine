#pragma once
#include"base.hpp"

namespace StarryEngine {
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
            const char* iconPath = nullptr;
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

        std::vector<unsigned char> mIconData;
    };
}