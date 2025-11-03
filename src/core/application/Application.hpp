#pragma once
#include "../../renderer/core/VulkanCore/VulkanCore.hpp"
#include "../../renderer/core/WindowContext/WindowContext.hpp"
#include "../../renderer/core/VulkanRenderer.hpp"
#include "../../renderer/resources/models/geometry/shape/Cube.hpp"
#include "../../renderer/resources/shaders/ShaderProgram.hpp"
#include <memory>

namespace StarryEngine {

    class Application {
    public:
        Application();
        ~Application();
        void run();
        void setupRenderGraph();
        void setupResources();
        void executeGeometryPass(CommandBuffer* cmdBuffer, RenderContext& context);
        void updateUniformBuffer(float time);

        // 添加缺失的方法声明
        void createVertexBuffer();
        void createIndexBuffer();
        void createDescriptorSets();
        void createGraphicsPipeline(ShaderProgram::Ptr shaderProgram);

    private:
        StarryEngine::Window::Ptr window;
        StarryEngine::VulkanCore::Ptr vulkanCore;
        StarryEngine::WindowContext::Ptr windowContext;
        std::unique_ptr<StarryEngine::VulkanRenderer> renderer;

        bool mFramebufferResized = false;

        // 更新成员变量类型 - 使用 ResourceHandle 而不是 VkBuffer
        std::shared_ptr<Cube> mCube;
        VkRenderPass mGeometryRenderPass = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> mGeometryFramebuffers;
        std::vector<VkDescriptorSet> mDescriptorSets;
        ResourceHandle mVertexBufferHandle;  // 改为 ResourceHandle
        ResourceHandle mIndexBufferHandle;   // 改为 ResourceHandle
        uint32_t mWidth = 800;
        uint32_t mHeight = 600;
    };

}