#pragma once
#include "../../renderer/core/VulkanCore/VulkanCore.hpp"
#include "../../renderer/core/WindowContext/WindowContext.hpp"
#include "../../renderer/VulkanRenderer.hpp"
#include "../../renderer/resources/models/mesh/Mesh.hpp"
#include "../../renderer/resources/shaders/ShaderProgram.hpp"
#include <memory>

namespace StarryEngine {
    class Application {
    public:
        Application();
        ~Application();

        void run();

    private:
        void setupPipelineResource();

        void setupRenderGraph();

        // 管线创建方法
        std::shared_ptr<Pipeline> createGeometryPipeline(PipelineBuilder& builder);

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

        ShaderProgram::Ptr shaderProgram;
        Mesh mesh;
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    };
}