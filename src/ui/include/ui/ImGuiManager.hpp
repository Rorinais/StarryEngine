#pragma once

#include <imgui.h>
#include <memory>
#include <functional>
#include <vector>
#include <string>

#include <renderer/interface/IRHI.hpp>
#include <renderer/interface/RHIEnums.hpp>
#include <renderer/interface/IRHI.hpp>
#include <renderer/interface/RHIEnums.hpp>
#include <renderer/interface/RHIStructs.hpp>
#include <renderer/interface/IRHIResources.hpp>
#include <renderer/interface/RHIFactory.hpp>
#include <renderer/interface/RHIManager.hpp>
#include <renderer/graph/RenderGraph.hpp>

namespace StarryEngine {
    class Window;

    class ImGuiManager {
    public:
        ImGuiManager();
        ~ImGuiManager();

        bool initialize(
            RHI::IRHI* rhi,
            RHI::ResourceManager* resMgr,
            std::shared_ptr<Window> window,
            uint32_t                width,
            uint32_t                height,
            uint32_t                swapchainImageCount,
            RHI::Format             swapchainFormat,
            const RHI::DescriptorPoolHandle& globalDescriptorPool
        );

        bool initializeVulkanBackend(
            RHI::IRHI* rhi,
            RHI::ResourceManager* resMgr,
            RHI::RenderPassHandle   guiRenderPass,
            uint32_t                imageCount
        );

        void shutdown(RHI::ResourceManager* resMgr);
        void shutdownVulkanBackend();
        void beginFrame();
        void endFrame();
        void render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex);
        void onKeyEvent(int glfwKey, int scancode, int action, int mods);
        void onMouseButtonEvent(int button, int action, int mods);
        void onMouseMoveEvent(float x, float y);
        void onMouseScrollEvent(float xOffset, float yOffset);
        void onWindowResize(uint32_t width, uint32_t height);
        void setDefaultFont(const std::string& fontPath = "");
        void setDarkTheme();

        ImGuiContext* getContext() const { return m_context; }

        bool isInitialized() const {
            return m_glfwInitialized && m_vulkanBackendReady;
        }

        bool isGlfwReady() const { return m_glfwInitialized; }

        bool isVulkanReady() const { return m_vulkanBackendReady; }

        static void SetCurrent(ImGuiManager* mgr) {
            if (mgr && mgr->m_context)
                ImGui::SetCurrentContext(mgr->m_context);
        }

        void setRenderGraph(std::shared_ptr<RenderGraph::RenderGraph> rdg) {
            m_rdg = rdg;
        }
        void setSubpassIndex(uint32_t subpassIdx) { m_subpassIndex = subpassIdx; }
        void setDefaultSampler(RHI::SamplerHandle sampler) {
            m_defaultSampler = sampler;
        }
        ImTextureID getSceneTextureID() const { return m_sceneTextureID; }

        void registerSceneTexture(RHI::ResourceManager* resMgr);

    private:
        void createDescriptorPool(RHI::ResourceManager* resMgr, uint32_t imageCount);
        void setupVulkanInitInfo(
            RHI::IRHI* rhi,
            RHI::ResourceManager* resMgr,
            std::shared_ptr<Window> window,
            uint32_t                width,
            uint32_t                height,
            uint32_t                imageCount,
            RHI::Format             swapchainFormat,
            const RHI::DescriptorPoolHandle& globalDescriptorPool
        );

        ImGuiContext* m_context = nullptr;

        bool m_glfwInitialized = false;
        bool m_vulkanBackendReady = false;

        RHI::DescriptorPoolHandle m_descriptorPool;
        RHI::TextureHandle        m_fontTexture;

        uint32_t m_width = 0;
        uint32_t m_height = 0;

        RHI::IRHI* m_rhi = nullptr;
        VkDevice m_vkDevice = VK_NULL_HANDLE;
        uint32_t m_graphicsQueueFamily = 0;
        VkQueue  m_graphicsQueue = VK_NULL_HANDLE;
        std::shared_ptr<Window> m_window;
        std::shared_ptr<RenderGraph::RenderGraph> m_rdg;
        RHI::SamplerHandle m_defaultSampler;        
        ImTextureID m_sceneTextureID = 0;
        uint32_t m_subpassIndex = 0;

        bool m_initInfoStored = false;
    };

} // namespace StarryEngine