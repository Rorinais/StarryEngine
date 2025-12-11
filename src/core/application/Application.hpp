#pragma once
#include "../../renderer/backends/vulkan/vulkanCore/VulkanCore.hpp"
#include "../../renderer/backends/vulkan/windowContext/WindowContext.hpp"
#include "../../renderer/backends/vulkan/SimpleVulkanBackend.hpp"
#include "../../renderer/resourceManager/models/mesh/Mesh.hpp"
#include "../../renderer/resourceManager/shaders/ShaderProgram.hpp"
#include "../../renderer/backends/vulkan/descriptor/DescriptorManager.hpp"
#include "../../renderer/backends/vulkan/renderPass/RenderPassBuilder.hpp"
#include "../../renderer/resourceManager/textures/Texture.hpp"
#include "../../renderer/resourceManager/buffers/UniformBuffer.hpp"
#include "../../renderer/backends/vulkan/pipeline/NewPipelineBuilder.hpp"
#include "../../renderer/backends/vulkan/pipeline/Pipeline.hpp"
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
        // 初始化方法
        void initialize();
        void createShaderProgram();
        void createRenderPass();
        void createDescriptorManager();
        void createDepthTexture();
        void createGraphicsPipeline();
        void createFramebuffers();

        // 组件注册方法
        void registerDefaultComponents();

        // 渲染方法
        void drawFrame();
        void recordCommandBuffer(RenderContext& context, uint32_t imageIndex);
        void updateUniformBuffer(uint32_t currentFrame);

        // 清理方法
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
        std::vector<VkFramebuffer> mSwapchainFramebuffers;
        Texture::Ptr mDepthTexture;

        // 着色器
        ShaderProgram::Ptr mShaderProgram;
        Mesh mMesh;

        // 描述符和Uniform Buffer
        DescriptorManager::Ptr mDescriptorManager;
        std::vector<UniformBuffer::Ptr> mMatrixUniformBuffers;
        std::vector<UniformBuffer::Ptr> mColorUniformBuffers;

        // 新的管线构建系统
        std::shared_ptr<ComponentRegistry> mComponentRegistry;
        std::shared_ptr<PipelineBuilder> mPipelineBuilder;

        // 管线和布局
        VkPipeline mGraphicsPipeline = VK_NULL_HANDLE;
        std::shared_ptr<PipelineLayout> mPipelineLayout;

    };

} // namespace StarryEngine