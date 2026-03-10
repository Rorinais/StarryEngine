#pragma once
#include "IRenderpass.hpp"
#include "../../subpassRecorder/GBufferRecorder.hpp"
#include "../../../core/Window.hpp"
#include "../../../logging/Logger.hpp"  

namespace StarryEngine::RenderGraph {
    class ImguiPass : public IRenderPass {
    public:
        ImguiPass(std::shared_ptr<RHI::IRHI> rhi, Window::Ptr window, RHI::DescriptorPoolHandle descriptorPool)
            : m_rhi(rhi), m_window(window), m_resMgr(rhi->getResourceManager()), m_descriptorPool(descriptorPool) {
            m_imguiRecorder = std::make_shared<ImGuiRecorder>(m_resMgr);
        }

        void setup(RenderGraph& renderGraph, TextureId input, TextureId output,
            RHI::ImageLayout depthInitial, RHI::ImageLayout depthFinal,
            RHI::ImageLayout colorInitial, RHI::ImageLayout colorFinal) override {
            m_inputTexture = input;
            m_renderGraph = &renderGraph;
            auto* passNode = renderGraph.addPassNode("ImGuiPass");
            passNode->setRenderArea(width, height);

            passNode->addColorOutput(output)
                .setLoadOp(RHI::AttachmentLoadOp::Clear)
                .setStoreOp(RHI::AttachmentStoreOp::Store)
                .setClearColor({ 0.0f, 0.0f, 0.0f, 1.0f })
                .setInitialLayout(colorInitial)  
                .setFinalLayout(colorFinal);      

            passNode->addInput(input)
                .setLoadOp(RHI::AttachmentLoadOp::Load)
                .setStoreOp(RHI::AttachmentStoreOp::DontCare)
                .setInitialLayout(depthInitial)
                .setFinalLayout(depthFinal);

            auto subpass = passNode->addSubpassProxy("ImGuiSubpass");
            subpass.addColorAttachment(output)
                .setRecorder(m_imguiRecorder.get());

            m_passNode = passNode;
        }

        void postCompile() {
            if (!m_passNode) return;
            auto* rpObj = m_resMgr->getRenderPass(m_passNode->getRenderPassHandle());
            if (!rpObj) return;
            VkRenderPass imguiRenderPass = static_cast<VkRenderPass>(rpObj->getNativeHandle());

            auto* pool = m_resMgr->getDescriptorPool(m_descriptorPool);
            if (!pool) return;
            VkDescriptorPool descPool = static_cast<VkDescriptorPool>(pool->getNativeHandle());

            m_imguiRecorder->init(m_rhi, descPool, m_window->getHandle(), imguiRenderPass);

            RHI::TextureHandle texHandle = m_renderGraph->getPhysicalTextureHandle(m_inputTexture);
            m_imguiRecorder->setDisplayTexture(texHandle);
        }

        void update(const Uniforms& /*data*/) override {
            m_imguiRecorder->newFrame();

            // 创建主 DockSpace 宿主窗口（与您原有代码相同）
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);

            ImGuiWindowFlags host_window_flags = 0;
            host_window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            host_window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
            host_window_flags |= ImGuiWindowFlags_NoDocking;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

            ImGui::Begin("MainDockSpaceWindow", nullptr, host_window_flags);
            ImGui::PopStyleVar(2);

            ImGuiID dockspace_id = ImGui::GetID("MyMainDockSpace");
            ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

            ImGui::End(); // 结束宿主窗口

            // -------------------- 在此之后绘制所有可停靠窗口 --------------------

            // 1. 已有的场景视图窗口
            m_imguiRecorder->drawTextureWindow("Scene Viewer");

            // 2. 新增控制面板窗口
            drawControlPanel("Control Panel");

            // 3. 新增日志窗口
            drawLogWindow("Log");

            // 如果您有更多窗口，可以继续添加...
            // drawAnotherWindow("Another Window");
        }

        // 新增的控制面板窗口绘制函数
        void drawControlPanel(const char* title) {
            ImGui::Begin(title);
            ImGui::Text("Rendering Settings");
            ImGui::SliderFloat("Exposure", &m_exposure, 0.1f, 5.0f);
            ImGui::ColorEdit3("Clear Color", (float*)&m_clearColor);
            if (ImGui::Button("Apply")) {
                // 将新值应用到渲染管线（例如更新 Uniform Buffer 或全局设置）
            }
            ImGui::End();
        }

        void drawLogWindow(const char* title) {
            auto sink = Logger::getImGuiSink();
            if (!sink) return;

            auto logs = sink->getLogs();  // 获取日志副本（线程安全）

            ImGui::Begin(title);
            ImGui::Text("Application Log:");
            ImGui::Separator();

            // 可选的过滤输入框
            static char filterBuffer[256] = "";
            ImGui::InputText("Filter", filterBuffer, IM_ARRAYSIZE(filterBuffer));
            std::string filter = filterBuffer;

            ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

            for (const auto& line : logs) {
                if (!filter.empty() && line.find(filter) == std::string::npos)
                    continue;
                ImGui::TextUnformatted(line.c_str());
            }

            // 自动滚动到底部（如果当前在底部）
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }

            ImGui::EndChild();
            ImGui::End();
        }

    private:
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        RHI::DescriptorPoolHandle m_descriptorPool;
        std::shared_ptr<ImGuiRecorder> m_imguiRecorder;
        std::shared_ptr<RHI::IRHI> m_rhi;
        Window::Ptr m_window;
        TextureId m_inputTexture;
        PassNode* m_passNode = nullptr;
        RenderGraph* m_renderGraph = nullptr;
        float m_exposure = 1.0f;
        glm::vec4 m_clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
        std::vector<std::string> m_logMessages;
    };
}
