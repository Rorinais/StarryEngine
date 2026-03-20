#pragma once
#include"ISubpassRecorder.hpp"
#include "../graph/PassNode.hpp"
#include "../backend/vulkan/VulkanRHI.hpp"
#include "../../core/base.hpp"


namespace StarryEngine::RenderGraph {
    class MeshDrawRecorder : public ISubpassRecorder {
    public:
        using ISubpassRecorder::ISubpassRecorder;

        void setPipelines(const std::vector<RHI::PipelineHandle>& pipelines) { m_pipelines = pipelines; }
        void setDrawItems(const std::vector<std::shared_ptr<Scene::DrawItem>>& items) { m_drawItems = items; }

        void recordCommands(RHI::RHICommandEncoder* encoder,
            const PassContext& pctx,
            uint32_t subpassIndex,
            uint32_t frameIndex) override {
            for (const auto& item : m_drawItems) {
                if (item->pipelineIndex >= m_pipelines.size()) continue;

                glm::mat4 transform;
                auto obj = item->object.lock();
                if (!obj) continue; // 物体已销毁，跳过绘制
                transform = obj->transform;

                auto pipeline = pctx.getResourceManager()->getPipeline(m_pipelines[item->pipelineIndex]);
                encoder->bindPipeline(pipeline);

                auto pipelineLayout = pctx.getResourceManager()->getPipelineLayout(pipeline->getLayout());
                // 绑定所有存在的描述符集
                for (const auto& [setIndex, setHandle] : item->descriptorSets) {
                    encoder->bindDescriptorSets(RHI::PipelineBindPoint::Graphics,
                        pipelineLayout, setIndex, { setHandle }, {});
                }

                encoder->pushConstants(pipelineLayout, RHI::ShaderStage::Vertex,
                    0, sizeof(glm::mat4), &transform);

                encoder->bindVertexBuffers(0, { pctx.getResourceManager()->getBuffer(item->vertexBuffer) }, { 0 });
                encoder->bindIndexBuffer(pctx.getResourceManager()->getBuffer(item->indexBuffer), 0, RHI::IndexType::UInt32);
                encoder->drawIndexed(item->indexCount, 1, item->indexOffset, 0, 0);
            }
        }

    private:
        std::vector<std::shared_ptr<Scene::DrawItem>> m_drawItems;
        std::vector<RHI::PipelineHandle> m_pipelines;  
        RHI::PipelineLayoutHandle m_pipelineLayout;
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

            SetupImGuiDarkStyle_Final();

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

        void SetupImGuiDarkStyle_Final()
        {
            ImGuiStyle& style = ImGui::GetStyle();
            ImVec4* colors = style.Colors;

            // ----- 使用推荐的暗色系配色 -----
            // 背景色 #01072c
            const ImVec4 bg_main = ImVec4(0.004f, 0.027f, 0.073f, 1.00f);
            // 卡片/面板背景 #1c2b60
            const ImVec4 bg_card = ImVec4(0.110f, 0.169f, 0.376f, 1.00f);
            // 边框/次要文字 #a4adc2
            const ImVec4 border = ImVec4(0.643f, 0.678f, 0.761f, 1.00f);
            // 主色 #39599b
            const ImVec4 primary = ImVec4(0.224f, 0.349f, 0.608f, 1.00f);
            // 主色悬停状态（稍亮） #6880b8
            const ImVec4 primary_hover = ImVec4(0.408f, 0.502f, 0.722f, 1.00f);
            // 主色激活状态（稍暗） #2a4070（从列表选取接近色 #2a3f6e? 但这里用 #354475 作为替代）
            const ImVec4 primary_active = ImVec4(0.208f, 0.267f, 0.459f, 1.00f);
            // 辅助色/强调色 #a160a5
            const ImVec4 accent = ImVec4(0.631f, 0.376f, 0.647f, 1.00f);
            // 主要文字 #ebf2fa
            const ImVec4 text_main = ImVec4(0.922f, 0.949f, 0.980f, 1.00f);
            // 标题文字（纯白） #ffffff
            const ImVec4 text_title = ImVec4(1.000f, 1.000f, 1.000f, 1.00f);
            // 次要文字/提示 #a4adc2（与边框相同，但可独立定义）
            const ImVec4 text_secondary = ImVec4(0.643f, 0.678f, 0.761f, 1.00f);

            // ----- ImGui 颜色映射 -----
            colors[ImGuiCol_WindowBg] = bg_main;
            colors[ImGuiCol_ChildBg] = bg_card;
            colors[ImGuiCol_PopupBg] = bg_card;
            colors[ImGuiCol_MenuBarBg] = bg_card;

            colors[ImGuiCol_TitleBg] = bg_card;
            colors[ImGuiCol_TitleBgActive] = primary;
            colors[ImGuiCol_TitleBgCollapsed] = bg_card;

            colors[ImGuiCol_Border] = border;
            colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
            colors[ImGuiCol_Separator] = border;
            colors[ImGuiCol_SeparatorHovered] = primary_hover;
            colors[ImGuiCol_SeparatorActive] = primary_active;

            colors[ImGuiCol_Text] = text_main;
            colors[ImGuiCol_TextDisabled] = text_secondary;
            colors[ImGuiCol_TextSelectedBg] = primary;

            colors[ImGuiCol_FrameBg] = bg_card;
            colors[ImGuiCol_FrameBgHovered] = primary;
            colors[ImGuiCol_FrameBgActive] = primary_hover;

            colors[ImGuiCol_Button] = primary;
            colors[ImGuiCol_ButtonHovered] = primary_hover;
            colors[ImGuiCol_ButtonActive] = primary_active;

            colors[ImGuiCol_Header] = primary;
            colors[ImGuiCol_HeaderHovered] = primary_hover;
            colors[ImGuiCol_HeaderActive] = primary_active;

            colors[ImGuiCol_ScrollbarBg] = bg_card;
            colors[ImGuiCol_ScrollbarGrab] = primary;
            colors[ImGuiCol_ScrollbarGrabHovered] = primary_hover;
            colors[ImGuiCol_ScrollbarGrabActive] = primary_active;

            colors[ImGuiCol_SliderGrab] = primary;
            colors[ImGuiCol_SliderGrabActive] = primary_hover;
            colors[ImGuiCol_CheckMark] = text_title;

            colors[ImGuiCol_Tab] = bg_card;
            colors[ImGuiCol_TabHovered] = primary_hover;
            colors[ImGuiCol_TabActive] = primary;
            colors[ImGuiCol_TabUnfocused] = bg_card;
            colors[ImGuiCol_TabUnfocusedActive] = primary;

            colors[ImGuiCol_DockingPreview] = accent;
            colors[ImGuiCol_DockingEmptyBg] = bg_card;

            colors[ImGuiCol_TableHeaderBg] = bg_card;
            colors[ImGuiCol_TableBorderStrong] = border;
            colors[ImGuiCol_TableBorderLight] = border;
            colors[ImGuiCol_TableRowBg] = bg_main;
            colors[ImGuiCol_TableRowBgAlt] = bg_card;

            colors[ImGuiCol_PlotLines] = text_secondary;
            colors[ImGuiCol_PlotLinesHovered] = primary_hover;
            colors[ImGuiCol_PlotHistogram] = accent;
            colors[ImGuiCol_PlotHistogramHovered] = primary_hover;

            colors[ImGuiCol_NavHighlight] = accent;
            colors[ImGuiCol_NavWindowingHighlight] = accent;
            colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.6f);

            colors[ImGuiCol_DragDropTarget] = accent;
            colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.7f);

            // ----- 样式微调（保持原样）-----
            style.FrameRounding = 4.0f;
            style.WindowRounding = 8.0f;
            style.ChildRounding = 4.0f;
            style.PopupRounding = 4.0f;
            style.GrabRounding = 4.0f;
            style.ScrollbarRounding = 4.0f;
            style.TabRounding = 4.0f;

            style.FrameBorderSize = 1.0f;
            style.WindowBorderSize = 1.0f;
            style.PopupBorderSize = 1.0f;
            style.ChildBorderSize = 1.0f;

            style.WindowPadding = ImVec2(8, 8);
            style.FramePadding = ImVec2(4, 3);
            style.ItemSpacing = ImVec2(8, 4);
            style.ItemInnerSpacing = ImVec2(4, 4);
            style.ScrollbarSize = 14.0f;
            style.GrabMinSize = 10.0f;
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
