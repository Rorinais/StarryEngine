#pragma once
#include "../../renderer/core/VulkanCore/VulkanCore.hpp"
#include "../../renderer/core/WindowContext/WindowContext.hpp"
#include "../../renderer/core/VulkanRenderer.hpp"
#include "../../renderer/resources/models/geometry/shape/Cube.hpp"
#include "../../renderer/resources/shaders/ShaderProgram.hpp"
#include "../../renderer/core/IVulkanBackend.hpp"
#include <memory>

namespace StarryEngine {
    class Application {
    public:
        Application();
        ~Application();

        void run();

    private:
        void setupResources();
        void setupRenderGraph();

        // 管线创建方法
        Pipeline::Ptr createGeometryPipeline(PipelineBuilder& builder);
        VertexInput getCubeVertexInput() const;
        Rasterization getRasterizationState() const;
        DepthStencil getDepthStencilState() const;
        ColorBlend getColorBlendState() const;
        InputAssembly getInputAssemblyState() const;
        MultiSample getMultisampleState() const;
        Dynamic getDynamicState() const;
        Viewport getViewportState() const;
        std::vector<VkDescriptorSetLayout> getDescriptorSetLayouts() const;

        // 渲染通道执行方法
        void executeGeometryPass(CommandBuffer* cmdBuffer, RenderContext& context);

        // 更新方法
        void updateUniformBuffer(float time);

    private:
        uint32_t mWidth = 800;
        uint32_t mHeight = 600;
        bool mFramebufferResized = false;

        Window::Ptr window;
        VulkanCore::Ptr vulkanCore;
        WindowContext::Ptr windowContext;
        std::unique_ptr<VulkanRenderer> renderer;

        // 缓冲区句柄
        ResourceHandle mVertexBufferHandle;
        ResourceHandle mIndexBufferHandle;

        // 着色器程序
        ShaderProgram::Ptr mShaderProgram;

        // 描述符集
        std::vector<VkDescriptorSet> mDescriptorSets;
        VkDescriptorSetLayout mDescriptorSetLayout = VK_NULL_HANDLE;

        // 渲染通道和管线
        VkRenderPass mGeometryRenderPass = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> mGeometryFramebuffers;
    };
}