#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <stdexcept>
#include <array>
#include <vector>
#include <memory>
#include <iostream>

#include "../../renderer/backends/vulkan/vulkanCore/VulkanCore.hpp"
#include "../../renderer/backends/vulkan/windowContext/WindowContext.hpp"
#include "../../renderer/backends/vulkan/VulkanBackend.hpp"
#include "../../renderer/backends/vulkan/descriptor/DescriptorManager.hpp"
#include "../../renderer/backends/vulkan/renderPass/RenderPassBuilder.hpp"
#include "../../renderer/backends/vulkan/renderPass/RenderPassBeginInfo.hpp" 
#include "../../renderer/backends/vulkan/pipeline/Pipeline.hpp"
#include "../../renderer/backends/vulkan/pipeline/NewPipelineBuilder.hpp"

#include "../../renderer/resource/models/mesh/Mesh.hpp"
#include "../../renderer/resource/models/ModelLoader.hpp"
#include "../../renderer/resource/models/geometry/shape/Cube.hpp"
#include "../../renderer/resource/shaders/ShaderBuilder.hpp"
#include "../../renderer/resource/shaders/ShaderProgram.hpp"
#include "../../renderer/resource/buffers/UniformBuffer.hpp"
#include "../../renderer/resource/buffers/VertexArrayBuffer.hpp"
#include "../../renderer/resource/buffers/IndexBuffer.hpp"
#include "../../renderer/resource/textures/Texture.hpp"
#include "../../renderer/VulkanRenderer.hpp"

namespace StarryEngine {
    // 前向声明
    struct MultiMaterialVertex;

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
        //void createFramebuffers();

        // 创建多材质立方体
        void createMultiMaterialCube();
        void createMultipleShaders();
        void createMultiplePipelines();

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
        std::shared_ptr<VulkanRenderer> mRenderer;
        LogicalDevice::Ptr mDevice;
        CommandPool::Ptr mCommandPool;
        CommandBuffer::Ptr mCommandBuffer;
        VkExtent2D mExtent={800 ,600 };

        // 渲染状态
        uint32_t mCurrentFrame = 0;
        bool mFramebufferResized = false;

        // 渲染资源
        std::unique_ptr<RenderPassBuildResult> mRenderPassResult;
        std::vector<VkFramebuffer> mSwapchainFramebuffers;
        Texture::Ptr mDepthTexture;
        RenderPassBeginInfo passBeginInfo; 

        // 着色器
        ShaderProgram::Ptr mShaderProgram;
        Mesh mMesh;
        std::shared_ptr<ModelLoader> loader;

        // 描述符和Uniform Buffer
        DescriptorManager::Ptr mDescriptorManager;
        
        std::vector<UniformBuffer::Ptr> mMatrixUniformBuffers;
        std::vector<UniformBuffer::Ptr> mColorUniformBuffers;

        // 管线构建系统
        std::shared_ptr<ComponentRegistry> mComponentRegistry;
        std::shared_ptr<PipelineBuilder> mPipelineBuilder;

        // 管线和布局
        VkPipeline mGraphicsPipeline = VK_NULL_HANDLE;
        std::shared_ptr<PipelineLayout> mPipelineLayout;

        // 多材质立方体相关
        VertexArrayBuffer::Ptr mMultiMaterialVAO;
        IndexBuffer::Ptr mMultiMaterialIBO;
        uint32_t mMultiMaterialIndexCount;
        std::vector<ShaderProgram::Ptr> mMultiMaterialShaders;
        std::vector<VkPipeline> mMultiMaterialPipelines;
        std::vector<std::vector<UniformBuffer::Ptr>> mMaterialColorBuffers;
        
        // 材质相关描述符
        std::shared_ptr<DescriptorManager> mMaterialDescriptorManager;
        std::shared_ptr<PipelineLayout> mMaterialPipelineLayout;
    };

} // namespace StarryEngine