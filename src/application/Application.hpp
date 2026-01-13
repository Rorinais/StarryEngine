#pragma once
#include "../base.hpp"
#include "Window.hpp"
#include "../renderer//backend/VulkanRHI.hpp"

// 确保包含了 RHI 类型
#include "../renderer/interface/RHI_STRUCTS_CONFIG.hpp"
#include "../renderer/interface/RHI_STRUCTS_BASE.hpp"

namespace StarryEngine {

    class Application {
    public:
        Application();
        ~Application();

        void run();

    private:
        // 窗口相关
        uint32_t m_width = 800;
        uint32_t m_height = 600;
        const char* m_title = "StarryEngine";
        const char* m_icon_path = "assets/icons/window_icon.png";
        Window::Ptr m_window;
        bool mFramebufferResized = false;

        std::shared_ptr<VulkanRHI> m_rhi;
    };

} // namespace StarryEngine