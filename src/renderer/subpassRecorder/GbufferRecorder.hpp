#pragma once
#include"ISubpassRecorder.hpp"
#include "../graph/PassNode.hpp"
#include "../backend/vulkan/VulkanRHI.hpp"
#include "../../core/base.hpp"


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
            if (m_rhi && m_sampler != VK_NULL_HANDLE) {
                vkDestroySampler(static_cast<VkDevice>(m_rhi->getDevice()), m_sampler, nullptr);
            }
        }

        inline void init(std::shared_ptr<RHI::IRHI> rhi, VkDescriptorPool descriptorPool, GLFWwindow* window, VkRenderPass renderPass) {
            m_rhi = rhi;
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
            io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
            
            // 初始化 GLFW 后端
            ImGui_ImplGlfw_InitForVulkan(window, true);

            // 获取 Vulkan 必要信息
            VkInstance instance = static_cast<VkInstance>(rhi->getInstance());
            VkPhysicalDevice physicalDevice = static_cast<VkPhysicalDevice>(rhi->getPhysicalDevice());
            VkDevice device = static_cast<VkDevice>(rhi->getDevice());
            uint32_t queueFamily = rhi->getGraphicsQueueFamilyIndex();
            VkQueue queue = static_cast<VkQueue>(rhi->getGraphicsQueue());
            uint32_t imageCount = rhi->getSwapChainImageCount();

            // 初始化 Vulkan 后端（新版结构体）
            ImGui_ImplVulkan_InitInfo init_info = {};
            init_info.Instance = instance;
            init_info.PhysicalDevice = physicalDevice;
            init_info.Device = device;
            init_info.QueueFamily = queueFamily;
            init_info.Queue = queue;
            init_info.DescriptorPool = descriptorPool;
            init_info.MinImageCount = imageCount;
            init_info.ImageCount = imageCount;
            init_info.UseDynamicRendering = false;
            init_info.CheckVkResultFn = [](VkResult err) {
                if (err != VK_SUCCESS) {
                    std::cerr << "[ImGui] Vulkan error: " << err << std::endl;
                }
                };

            // 将 RenderPass 等信息放入 PipelineInfoMain
            init_info.PipelineInfoMain.RenderPass = renderPass;
            init_info.PipelineInfoMain.Subpass = 0;
            init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

            // 初始化 ImGui Vulkan 后端
            ImGui_ImplVulkan_Init(&init_info);

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

        inline void drawTextureWindow(const char* title) {
            if (!m_initialized) return;

            // 设置窗口初始大小（仅在首次使用时生效）
            ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
            // 可选：设置窗口位置
            ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);

            ImGui::Begin(title);
            if (m_sceneTextureID) {
                ImVec2 viewportSize = ImGui::GetContentRegionAvail();
                ImGui::Image(m_sceneTextureID, viewportSize);
            }
            else {
                ImGui::Text("Texture not available (ID=%llu)", (unsigned long long)m_sceneTextureID);
            }
            ImGui::End();
        }

        inline void setDisplayTexture(RHI::TextureHandle textureHandle) {
            if (!m_initialized || !m_rhi) {
                std::cerr << "[ImGuiRecorder] Not initialized or missing RHI" << std::endl;
                return;
            }

            // 1. 创建采样器（如果尚未创建）
            if (m_sampler == VK_NULL_HANDLE) {
                VkSamplerCreateInfo samplerInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
                samplerInfo.magFilter = VK_FILTER_LINEAR;
                samplerInfo.minFilter = VK_FILTER_LINEAR;
                samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                samplerInfo.anisotropyEnable = VK_FALSE;
                samplerInfo.maxAnisotropy = 1.0f;
                samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
                samplerInfo.unnormalizedCoordinates = VK_FALSE;
                samplerInfo.compareEnable = VK_FALSE;
                samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
                samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
                samplerInfo.mipLodBias = 0.0f;
                samplerInfo.minLod = 0.0f;
                samplerInfo.maxLod = 0.0f;

                if (vkCreateSampler(static_cast<VkDevice>(m_rhi->getDevice()), &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
                    std::cerr << "[ImGuiRecorder] Failed to create sampler" << std::endl;
                    return;
                }
            }

            // 2. 从纹理句柄获取 ImageView
            if (!textureHandle.isValid()) {
                std::cerr << "[ImGuiRecorder] Invalid texture handle" << std::endl;
                m_sceneTextureID = 0;
                return;
            }
            auto* texture = mResMgr->getTexture(textureHandle);
            if (!texture) {
                std::cerr << "[ImGuiRecorder] Texture object is null" << std::endl;
                m_sceneTextureID = 0;
                return;
            }
            VkImageView imageView = static_cast<VkImageView>(texture->getDefaultView()); // 确保使用 getDefaultView()
            if (imageView == VK_NULL_HANDLE) {
                std::cerr << "[ImGuiRecorder] Texture default view is null" << std::endl;
                m_sceneTextureID = 0;
                return;
            }

            // 3. 生成 ImTextureID
            VkDescriptorSet descSet = ImGui_ImplVulkan_AddTexture(
                m_sampler,
                imageView,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );
            if (descSet == VK_NULL_HANDLE) {
                std::cerr << "[ImGuiRecorder] ImGui_ImplVulkan_AddTexture failed" << std::endl;
                m_sceneTextureID = 0;
                return;
            }
            m_sceneTextureID = reinterpret_cast<ImTextureID>(descSet);
            std::cout << "[ImGuiRecorder] Texture ID set to: " << m_sceneTextureID << std::endl;
        }

    private:
        bool m_initialized = false;
        std::shared_ptr<RHI::IRHI> m_rhi;
        VkSampler        m_sampler = VK_NULL_HANDLE;
        ImTextureID      m_sceneTextureID = 0;
    };
 
}
