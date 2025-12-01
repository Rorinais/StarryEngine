#pragma once
#include "../../renderer/backends/vulkan/vulkanCore/VulkanCore.hpp"
#include "../../renderer/backends/vulkan/windowContext/WindowContext.hpp"
#include "../../renderer/backends/vulkan/SimpleVulkanBackend.hpp"
#include "../../renderer/resourceManager/models/mesh/Mesh.hpp"
#include "../../renderer/resourceManager/shaders/ShaderProgram.hpp"
#include "../../renderer/backends/vulkan/pipeline/pipeline.hpp"
#include "../../renderer/backends/vulkan/descriptor/DescriptorManager.hpp"
#include "../../renderer/backends/vulkan/renderPass/RenderPassBuilder.hpp"
#include "../../renderer/resourceManager/textures/Texture.hpp"
#include "../../renderer/resourceManager/buffers/UniformBuffer.hpp"
#include <memory>

namespace StarryEngine {

    class Application {
    public:
        struct UniformBufferObject {
            alignas(16) glm::mat4 model;
            alignas(16) glm::mat4 view;
            alignas(16) glm::mat4 proj;
        };

        Application();
        ~Application();

        void run();

    private:
        void initialize();
        void createShaderProgram();
        void createRenderPass();
        void createDescriptorManager();
        void createUniformBuffers();  
        void createDepthTexture();   
        void createGraphicsPipeline();
        void createFramebuffers();

        void drawFrame();
        void recordCommandBuffer(RenderContext& context, uint32_t imageIndex);
        void updateUniformBuffer(uint32_t currentFrame);

        void cleanup();
        void cleanupSwapchain();
        void recreateSwapchain();

    private:
        // 常量
        static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

        // 窗口和核心
        Window::Ptr mWindow;
        VulkanCore::Ptr mVulkanCore;
        WindowContext::Ptr mWindowContext;
        SimpleVulkanBackend mVulkanBackend;

        // 渲染状态
        uint32_t mCurrentFrame = 0;
        bool mFramebufferResized = false;
        uint32_t mWidth = 800;
        uint32_t mHeight = 600;

        // 渲染资源
        std::unique_ptr<RenderPassBuildResult> mRenderPassResult;
        VkRenderPass mRenderPass = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> mSwapchainFramebuffers;
        Texture::Ptr mDepthTexture;  // 深度纹理

        // 管线和着色器
        Pipeline::Ptr mGraphicsPipeline;
        ShaderProgram::Ptr mShaderProgram;
        Mesh mMesh;

        // 描述符和Uniform Buffer
        DescriptorManager::Ptr mDescriptorManager;
        std::vector<UniformBuffer::Ptr> mUniformBuffers;  // 改为UniformBuffer指针

        // 同步对象
        std::vector<VkFence> mImagesInFlight;
    };

} // namespace StarryEngine