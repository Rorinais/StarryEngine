#pragma once
#include "../../renderer/core/VulkanCore/VulkanCore.hpp"
#include "../../renderer/core/WindowContext/WindowContext.hpp"
#include "../../renderer/core/VulkanRenderer.hpp"

namespace StarryEngine {

    class Application {
    public:
        Application();
        ~Application();
        void run();

    private:
        StarryEngine::Window::Ptr window;
        StarryEngine::VulkanCore::Ptr vulkanCore;
        StarryEngine::WindowContext::Ptr windowContext;
        StarryEngine::VulkanRenderer::Ptr renderer;

        bool mFramebufferResized = false;
    };

}