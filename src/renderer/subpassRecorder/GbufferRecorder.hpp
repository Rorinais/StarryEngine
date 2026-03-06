#pragma once
#include"ISubpassRecorder.hpp"
#include "../graph/PassNode.hpp"
#include "../backend/VulkanRHI.hpp"
#include "../../base.hpp"


namespace StarryEngine::RenderGraph {
    class GBufferRecorder : public ISubpassRecorder {
    public:
        using ISubpassRecorder::ISubpassRecorder;
        void recordCommands(RHI::RHICommandEncoder* encoder,
            const PassContext& pctx,
            uint32_t subpassIndex,
            uint32_t frameIndex) {

            // 1. 获取当前 Subpass 的 Pipeline
            auto pipeline = pctx.getPipeline(subpassIndex);
            if (!pipeline.isValid()) return;
            encoder->bindPipeline(mResMgr->getPipeline(pipeline));

            // 2. 绑定材质的描述符集
            auto descSet = mMaterial->getDescriptorSet();
            auto pipelineLayoutHandle = mMaterial->getPipelineLayout();
            auto* pipelineLayout = mResMgr->getPipelineLayout(pipelineLayoutHandle);
            uint32_t setIndex = mMaterial->getSetIndex();

            encoder->bindDescriptorSets(RHI::PipelineBindPoint::Graphics,
                pipelineLayout,
                setIndex,
                { descSet },
                {});

            // 3. 绑定所有顶点缓冲区（每个 binding 单独绑定）
            auto bindings = mGeometry->getBindings();
            for (uint32_t binding : bindings) {
                auto vbHandle = mGeometry->getVertexBufferHandle(binding);
                if (!vbHandle.isValid()) {
                    std::cerr << "[GBufferRecorder] Missing vertex buffer for binding " << binding << std::endl;
                    continue;
                }
                encoder->bindVertexBuffers(binding,
                    { mResMgr->getBuffer(vbHandle) },
                    { 0 });
            }

            // 4. 绑定索引缓冲区
            auto ibHandle = mGeometry->getIndexBufferHandle();
            if (!ibHandle.isValid()) {
                std::cerr << "[GBufferRecorder] Missing index buffer" << std::endl;
                return;
            }
            encoder->bindIndexBuffer(mResMgr->getBuffer(ibHandle),
                0,
                RHI::IndexType::UInt32);

            // 5. 绘制
            encoder->drawIndexed(mGeometry->getIndexCount(), 1, 0, 0, 0);
        }
        
    };

    class PostProcessRecorder : public ISubpassRecorder {
    public:
        using ISubpassRecorder::ISubpassRecorder;

        void recordCommands(RHI::RHICommandEncoder* encoder,
            const PassContext& pctx,
            uint32_t subpassIndex,
            uint32_t frameIndex) override {
            // 绘制全屏三角形
            auto pipeline = pctx.getPipeline(subpassIndex);
            if (!pipeline.isValid()) return;
            encoder->bindPipeline(mResMgr->getPipeline(pipeline));

            auto descSet = mMaterial->getDescriptorSet();
            auto pipelineLayoutHandle = mMaterial->getPipelineLayout();
            auto* pipelineLayout = mResMgr->getPipelineLayout(pipelineLayoutHandle);
            encoder->bindDescriptorSets(RHI::PipelineBindPoint::Graphics,
                pipelineLayout,
                mMaterial->getSetIndex(),
                { descSet }, {});

            encoder->draw(3, 1, 0, 0);
        }

        void updateInputAttachment(uint32_t binding, RHI::TextureHandle texture, RHI::ImageLayout layout = RHI::ImageLayout::ShaderReadOnly) {
            mMaterial->updateInputAttachment(binding, texture, layout);
        }
    };

    class GridRecorder : public ISubpassRecorder {
    public:
        using ISubpassRecorder::ISubpassRecorder;

        void recordCommands(RHI::RHICommandEncoder* encoder,
            const PassContext& pctx,
            uint32_t subpassIndex,
            uint32_t frameIndex) override {
            auto pipeline = pctx.getPipeline(subpassIndex);
            if (!pipeline.isValid()) return;
            encoder->bindPipeline(mResMgr->getPipeline(pipeline));

            // 绑定描述符集（如果有 UniformBuffer）
            auto descSet = mMaterial->getDescriptorSet();
            auto pipelineLayoutHandle = mMaterial->getPipelineLayout();
            auto* pipelineLayout = mResMgr->getPipelineLayout(pipelineLayoutHandle);
            encoder->bindDescriptorSets(RHI::PipelineBindPoint::Graphics,
                pipelineLayout,
                mMaterial->getSetIndex(),
                { descSet }, {});

            // 绑定顶点缓冲区
            auto bindings = mGeometry->getBindings();
            for (uint32_t binding : bindings) {
                auto vbHandle = mGeometry->getVertexBufferHandle(binding);
                if (!vbHandle.isValid()) continue;
                encoder->bindVertexBuffers(binding,
                    { mResMgr->getBuffer(vbHandle) },
                    { 0 });
            }

            // 绑定索引缓冲区
            auto ibHandle = mGeometry->getIndexBufferHandle();
            if (ibHandle.isValid()) {
                encoder->bindIndexBuffer(mResMgr->getBuffer(ibHandle),
                    0,
                    RHI::IndexType::UInt32);
                encoder->drawIndexed(mGeometry->getIndexCount(), 1, 0, 0, 0);
            }
            else {
                // 如果没有索引缓冲区，直接绘制顶点数量（假设顶点数据为线列表）
                encoder->draw(mGeometry->getVertexCount(), 1, 0, 0);
            }
        }
    };

    class ImGuiRecorder : public ISubpassRecorder {
    public:
        using ISubpassRecorder::ISubpassRecorder;

        ~ImGuiRecorder() {
            if (m_initialized) {
                ImGui_ImplVulkan_Shutdown();
                ImGui_ImplGlfw_Shutdown();
                ImGui::DestroyContext();
            }
        }

        inline void init(VulkanRHI* rhi, VkDescriptorPool descriptorPool, GLFWwindow* window, VkRenderPass renderPass) {
            // 如果已经初始化，先清理旧的 ImGui 资源
            if (m_initialized) {
                ImGui_ImplVulkan_Shutdown();
                ImGui_ImplGlfw_Shutdown();
                ImGui::DestroyContext();
                m_initialized = false;
            }

            // 重新创建 ImGui 上下文
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO(); (void)io;
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

            // 初始化 GLFW 后端
            ImGui_ImplGlfw_InitForVulkan(window, true);

            // 获取 Vulkan 必要信息
            VkInstance instance = rhi->getInstance();
            VkPhysicalDevice physicalDevice = rhi->getPhysicalDevice();
            VkDevice device = rhi->getDevice();
            uint32_t queueFamily = rhi->getGraphicsQueueFamilyIndex();
            VkQueue queue = rhi->getGraphicsQueue();
            uint32_t imageCount = rhi->getSwapChainImageCount();

            // 初始化 Vulkan 后端
            ImGui_ImplVulkan_InitInfo init_info = {};
            init_info.Instance = instance;
            init_info.PhysicalDevice = physicalDevice;
            init_info.Device = device;
            init_info.QueueFamily = queueFamily;
            init_info.Queue = queue;
            init_info.DescriptorPool = descriptorPool;
            init_info.RenderPass = renderPass;  // 使用传入的 RenderPass
            init_info.MinImageCount = imageCount;
            init_info.ImageCount = imageCount;
            init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
            init_info.CheckVkResultFn = [](VkResult err) {
                if (err != VK_SUCCESS) {
                    std::cerr << "[ImGui] Vulkan error: " << err << std::endl;
                }
                };

            ImGui_ImplVulkan_Init(&init_info);
            ImGui_ImplVulkan_CreateFontsTexture();

            m_initialized = true;
        }

        inline void newFrame() {
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
        }

        inline void recordCommands(RHI::RHICommandEncoder* encoder,
            const PassContext& ctx,
            uint32_t subpassIndex,
            uint32_t frameIndex) {
            if (!m_initialized) return;
            ImGui::Render();
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), static_cast<VkCommandBuffer>(encoder->getCommandBuffer()));
        }
    private:
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        bool m_initialized = false;
    };
 
}
