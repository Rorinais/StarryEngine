#pragma once
#ifdef __linux__
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <dlfcn.h>
#endif

#ifdef _WIN32
    #include <windows.h>
    #include <commdlg.h>
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include <GLFW/glfw3native.h>
#endif

#include <filesystem>
#include <chrono>

#include "../event/Events.hpp"
#include "../logging/Logger.hpp"
#include "../core/FrameMonitor.hpp"
#include "../renderer/Renderer.hpp"
#include "../renderer/graph/RenderGraph.hpp"
#include "../renderer/backend/RHIFactory.hpp"
#include "../renderer/interface/RHI_TYPES.hpp"
#include "../scene/camera/CameraController.hpp"
#include "../renderer/subpassRecorder/GbufferRecorder.hpp"
#include "../renderer/subpassRecorder/DeferredLightingRecorder.hpp"

#include "../ui/ImGuiManager.hpp"
#include "../ui/ImGuiRecorder.hpp"
#include "../assets/Assets.hpp"

#include "TextEditor.h"
#include "ImGuiFileDialog.h"  


namespace StarryEngine {
    class Application {
    public:
        Application();
        ~Application();

        void run();
        void createDescriptorPool();
        void initEventDispatcher();
        void setRenderer(std::shared_ptr<Renderer> renderer) { m_renderer = renderer; }
        void setScene(std::shared_ptr<Scene::Scene> scene) { m_scene = scene; }

        std::shared_ptr<RHI::IRHI> getRenderHardwareInterface() { return m_rhi; }
        std::shared_ptr<RHI::ResourceManager> getResourceManager() { return m_resMgr; }
        RHI::DescriptorPoolHandle getGlobalDescriptorPool() { return m_descriptorPool; }
        uint32_t getWidth() { return m_width; }
        uint32_t getHeight() { return m_height; }

        ImGuiManager* getImGuiManager() { return m_imguiManager.get(); }
        bool isImGuiEnabled() const { return m_imguiEnabled; }

        // std::string OpenFileDialog() {
        //     OPENFILENAMEA ofn;
        //     char szFile[260] = { 0 };
        //     ZeroMemory(&ofn, sizeof(ofn));
        //     ofn.lStructSize = sizeof(ofn);
        //     ofn.hwndOwner = glfwGetWin32Window(m_window->getHandle());
        //     ofn.lpstrFile = szFile;
        //     ofn.nMaxFile = sizeof(szFile);
        //     ofn.lpstrFilter = "Shader Files\0*.vert;*.frag;*.comp;*.glsl\0All\0*.*\0";
        //     ofn.nFilterIndex = 1;
        //     ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

        //     if (GetOpenFileNameA(&ofn) == TRUE) {
        //         return std::string(ofn.lpstrFile);
        //     }
        //     return "";
        // }

        //void initComputePipeline() {
        //    auto* resMgr = m_resMgr.get();

        //    // --- 创建 Storage Buffer (1024 个 float) ---
        //    RHI::BufferDesc bufferDesc;
        //    bufferDesc.size = sizeof(float) * 1024;
        //    bufferDesc.type = RHI::BufferType::Storage;
        //    bufferDesc.memoryType = RHI::MemoryType::CPU_To_GPU;   // GPU 本地内存
        //    bufferDesc.debugName = "ComputeDataBuffer";

        //    RHI::BufferDesc stagingDesc;
        //    stagingDesc.size = sizeof(float) * 1024;
        //    stagingDesc.type = RHI::BufferType::Staging; // 或者 Storage
        //    stagingDesc.memoryType = RHI::MemoryType::GPU_To_CPU; // CPU可读
        //    m_computeStaging = m_resMgr->createBuffer(stagingDesc, "ComputeStaging");

        //    // 初始数据（可选），这里假设全部置零
        //    std::vector<float> initialData(1024, 1.0f);
        //    bufferDesc.initialData = initialData.data();
        //    bufferDesc.initialDataSize = initialData.size() * sizeof(float);

        //    m_computeBuffer = resMgr->createBuffer(bufferDesc, "ComputeDataBuffer");

        //    // --- 创建描述符集布局（binding=0, Storage Buffer, Compute stage） ---
        //    RHI::DescriptorSetLayoutDesc setLayoutDesc;
        //    setLayoutDesc.bindings.push_back({
        //        0,                                  // binding
        //        RHI::DescriptorType::StorageBuffer, // 类型
        //        1,                                  // count
        //        RHI::ShaderStage::Compute           // 阶段 (需确保枚举映射到 VK_SHADER_STAGE_COMPUTE_BIT)
        //        });
        //    m_computeDescriptorSetLayout = resMgr->createDescriptorSetLayout(setLayoutDesc, "ComputeSetLayout");

        //    // --- 创建管线布局 ---
        //    RHI::PipelineLayoutDesc pipeLayoutDesc;
        //    pipeLayoutDesc.descriptorSetLayouts.push_back(m_computeDescriptorSetLayout);
        //    m_computePipelineLayout = resMgr->createPipelineLayout(pipeLayoutDesc, "ComputePipeLayout");

        //    // --- 分配描述符集 ---
        //    RHI::DescriptorSetDesc setDesc;
        //    setDesc.descriptorPool = m_descriptorPool;                // 使用已有的全局描述符池
        //    setDesc.descriptorSetLayout = m_computeDescriptorSetLayout;
        //    m_computeDescriptorSet = resMgr->createDescriptorSet(setDesc, "ComputeSet");

        //    auto* descSet = m_resMgr->getDescriptorSet(m_computeDescriptorSet);  // 返回 RHIDescriptorSet*
        //    if (!descSet) {
        //        std::cerr << "Invalid descriptor set handle" << std::endl;
        //        return;
        //    }

        //    // 准备缓冲区信息
        //    RHI::DescriptorBufferInfo bufferInfo{};
        //    bufferInfo.buffer = m_computeBuffer;       // BufferHandle
        //    bufferInfo.offset = 0;
        //    bufferInfo.range = VK_WHOLE_SIZE;        // 或者具体大小

        //    descSet->writeBuffer(0, 0, m_resMgr->getBuffer(m_computeBuffer), 0, VK_WHOLE_SIZE);
        //    descSet->update();

        //    // --- 加载计算着色器 SPIR‑V ---
        //    Assets::ShaderLoader loader(m_resMgr);
        //    auto computeshader = loader.loadFromFile("assets/shaders/compute_test.comp",RHI::ShaderStage::Compute); 

        //    // --- 创建计算管线 ---
        //    RHI::ComputePipelineDesc pipeDesc;
        //    pipeDesc.computeShader = computeshader.value().module;
        //    pipeDesc.pipelineLayoutHandle = m_computePipelineLayout;
        //    pipeDesc.debugName = "MyComputePipeline";
        //    m_computePipeline = resMgr->createComputePipeline(pipeDesc, "MyComputePipeline");
        //}


    private:
        Window::Ptr m_window;
        uint32_t m_width = 1200;
        uint32_t m_height = 720;
        const char* m_title = "StarryEngine";
        const char* m_icon_path = "assets/icons/window_icon.png";
        bool m_framebufferResized = false;
        uint32_t m_flightFrame = 2;

        std::shared_ptr<Scene::Scene> m_scene;
        std::shared_ptr<Renderer> m_renderer;

        std::shared_ptr<RHI::IRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        RHI::DescriptorPoolHandle m_descriptorPool;

        std::unique_ptr<CameraController> m_cameraController;
        bool m_controlActive = false;
        bool m_enableControl = true;

        // ---------- 自动 Shader 热重载相关 ----------
        std::unordered_map<std::string, std::filesystem::file_time_type> m_shaderTimestamps;
        std::chrono::steady_clock::time_point m_lastFileCheck;
        std::chrono::steady_clock::time_point m_nextAllowedReload;
        static constexpr auto kReloadCooldown = std::chrono::milliseconds(1000); // 防抖冷却

        std::unique_ptr<ImGuiManager>                m_imguiManager;
        std::shared_ptr<ImGuiRecorder>               m_imguiRecorder;
        bool m_imguiEnabled = false;
        bool m_showDemoWindow = true;
        bool m_showPerformancePanel = true;
        bool m_showSceneGraph = false;
        bool m_showMaterialEditor = false;
        bool m_showDeveloperTools = true;
        bool m_showSceneView = true;
        bool m_showInspector = true;
        bool m_showCodeEditor = true;
        bool m_showConsole = true;
        void initImGui();
        void drawImGuiPanels(float deltaTime);


        // 代码编辑器相关
        TextEditor m_shaderEditor;
        std::string m_currentShaderPath;
        // 辅助函数
        void openShaderFile(const std::string& path);
        void saveCurrentShaderFile();

        //RHI::PipelineHandle       m_computePipeline;
        //RHI::PipelineLayoutHandle m_computePipelineLayout;
        //RHI::DescriptorSetLayoutHandle m_computeDescriptorSetLayout;
        //RHI::DescriptorSetHandle  m_computeDescriptorSet;
        //RHI::BufferHandle         m_computeBuffer;        // SSBO
        //RHI::BufferHandle         m_computeStaging;

    };
} // namespace StarryEngine