#pragma once 
#include "./interface/IRenderer.hpp"
#include "./interface/IBackend.hpp"
#include <memory>
#include <glm/glm.hpp>
#include <vector>

namespace StarryEngine {
    class Window;
    class VulkanRenderer : public IRenderer {
    public:
        VulkanRenderer() = default;
        ~VulkanRenderer() override { shutdown(); }
        
        // 初始化
        void init(std::shared_ptr<Window> window);
        void shutdown() override;
        
        // 渲染循环控制
        void beginFrame() override;
        void endFrame() override;
        bool shouldClose() const override;
        void pollEvents() override;
        void render() override;
        void onSwapchainRecreated() override;

        // 创建帧缓冲
        void createFramebuffers(std::vector<VkFramebuffer>& framebuffer, 
                                VkImageView depthTexture, 
                                VkRenderPass renderpass);
        
        // 获取内部对象
        template <class T>
        std::shared_ptr<T> getBackendAs() const {
            static_assert(std::is_base_of_v<IBackend, T>, "T must derive from IBackend");
            return std::dynamic_pointer_cast<T>(mBackend);
        }

    private:
        std::shared_ptr<IBackend> mBackend;
        VulkanCore::Ptr mVulkanCore;
        WindowContext::Ptr mWindowContext;
        std::shared_ptr<Window> mWindow;  // 存储窗口引用
    };
} // namespace StarryEngine