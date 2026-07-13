#include "ImGuiManager.hpp"
#include "../core/Window.hpp"
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <cstring>

namespace StarryEngine {

    // ════════════════════════════════════════
    // 构造 & 析构
    // ════════════════════════════════════════

    ImGuiManager::ImGuiManager() {
        IMGUI_CHECKVERSION();
        m_context = ImGui::CreateContext();
        ImGui::SetCurrentContext(m_context);

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.IniFilename = nullptr;

        setDarkTheme();
        setDefaultFont();
    }

    ImGuiManager::~ImGuiManager() {
        if (m_context) {
            ImGui::SetCurrentContext(m_context);   

            if (m_vulkanBackendReady) {
                ImGui_ImplVulkan_Shutdown();
                m_vulkanBackendReady = false;
            }
            if (m_glfwInitialized) {
                ImGui_ImplGlfw_Shutdown();
                m_glfwInitialized = false;
            }

            ImGui::DestroyContext(m_context);
            m_context = nullptr;
        }
    }

    // ════════════════════════════════════════
    // 初始化 — 第一阶段：GLFW + DescriptorPool
    // ════════════════════════════════════════

    bool ImGuiManager::initialize(
        RHI::IRHI* rhi,
        RHI::ResourceManager* resMgr,
        std::shared_ptr<Window> window,
        uint32_t                width,
        uint32_t                height,
        uint32_t                swapchainImageCount,
        RHI::Format             swapchainFormat,
        const RHI::DescriptorPoolHandle& globalDescriptorPool)
    {
        ImGui::SetCurrentContext(m_context);
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.IniFilename = "imgui.ini";

        m_width = width;
        m_height = height;
        m_window = window;

        // ── 1. ImGui_ImplGlfw 初始化 ──
        GLFWwindow* glfwWindow = window->getHandle();
        if (!ImGui_ImplGlfw_InitForVulkan(glfwWindow, true)) {
            return false;
        }
        m_glfwInitialized = true;

        // ── 2. 创建 DescriptorPool（Vulkan 后端暂不 init）──
        createDescriptorPool(resMgr, swapchainImageCount);

        // ── 3. 收集原生句柄（为后续 Vulkan init 做准备）──
        setupVulkanInitInfo(rhi, resMgr, window,
            width, height, swapchainImageCount,
            swapchainFormat, globalDescriptorPool);

        return true;
    }

    // ════════════════════════════════════════
    // 初始化 — 第二阶段：Vulkan（需要 RenderPass）
    // ════════════════════════════════════════

    bool ImGuiManager::initializeVulkanBackend(
        RHI::IRHI* rhi,
        RHI::ResourceManager* resMgr,
        RHI::RenderPassHandle   guiRenderPass,
        uint32_t                imageCount)
    {
        if (m_vulkanBackendReady) return true;
        if (!m_glfwInitialized)  return false;

        m_rhi = rhi;
        m_vkDevice = static_cast<VkDevice>(rhi->getDevice());
        m_graphicsQueueFamily = rhi->getGraphicsQueueFamilyIndex();
        m_graphicsQueue = static_cast<VkQueue>(rhi->getGraphicsQueue());
        

        VkInstance       instance = static_cast<VkInstance>(rhi->getInstance());
        VkPhysicalDevice physicalDevice = static_cast<VkPhysicalDevice>(rhi->getPhysicalDevice());
        VkDescriptorPool nativePool = static_cast<VkDescriptorPool>(
            resMgr->getDescriptorPool(m_descriptorPool)->getNativeHandle());
        VkRenderPass     nativeRP = static_cast<VkRenderPass>(
            resMgr->getRenderPass(guiRenderPass)->getNativeHandle());

        ImGui::SetCurrentContext(m_context);

        // ── 构建 InitInfo ──
        ImGui_ImplVulkan_InitInfo initInfo{};

        // 基础字段
        initInfo.Instance = instance;
        initInfo.PhysicalDevice = physicalDevice;
        initInfo.Device = m_vkDevice;
        initInfo.QueueFamily = m_graphicsQueueFamily;
        initInfo.Queue = m_graphicsQueue;
        initInfo.PipelineCache = VK_NULL_HANDLE;
        initInfo.DescriptorPool = nativePool;
        initInfo.MinImageCount = imageCount;
        initInfo.ImageCount = imageCount;
        initInfo.Allocator = nullptr;
        initInfo.CheckVkResultFn = nullptr;

        initInfo.PipelineInfoMain.RenderPass = nativeRP;
        initInfo.PipelineInfoMain.Subpass = 0;
        initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

        // 如果开启动态渲染，需要额外设置
        // initInfo.UseDynamicRendering = true;
        // initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = { ... };

        // ── 调用 ImGui_ImplVulkan_Init（新版本只接受一个参数）──
        if (!ImGui_ImplVulkan_Init(&initInfo)) {
            return false;
        }

        m_vulkanBackendReady = true;

        return true;
    }

    void ImGuiManager::shutdownVulkanBackend() {
        if (m_context && m_vulkanBackendReady) {
            ImGui::SetCurrentContext(m_context);
            ImGui_ImplVulkan_Shutdown();
            m_vulkanBackendReady = false;
            m_sceneTextureID = 0;
        }
    }

    void ImGuiManager::shutdown(RHI::ResourceManager* resMgr) {
        if (m_context) {
            ImGui::SetCurrentContext(m_context);

            if (m_vulkanBackendReady) {
                ImGui_ImplVulkan_Shutdown();
                m_vulkanBackendReady = false;
            }
            if (m_glfwInitialized) {
                ImGui_ImplGlfw_Shutdown();
                m_glfwInitialized = false;
            }

        }

        if (m_descriptorPool.isValid() && resMgr) {
            resMgr->destroy(m_descriptorPool);
        }
    }

    // ════════════════════════════════════════
    // 内部辅助
    // ════════════════════════════════════════

    void ImGuiManager::setupVulkanInitInfo(
        RHI::IRHI* rhi,
        RHI::ResourceManager* resMgr,
        std::shared_ptr<Window> window,
        uint32_t                width,
        uint32_t                height,
        uint32_t                imageCount,
        RHI::Format             swapchainFormat,
        const RHI::DescriptorPoolHandle& globalDescriptorPool)
    {
        // 这里暂时不需要做任何事
        // 原生句柄在 initializeVulkanBackend 中获取
        // 如果 RHI 的原生句柄在整个生命周期内不变，可以在构造函数里存下来
        m_initInfoStored = true;
    }

    void ImGuiManager::createDescriptorPool(RHI::ResourceManager* resMgr, uint32_t imageCount) {
        RHI::DescriptorPoolDesc poolDesc;
        poolDesc.maxSets = 2000;
        poolDesc.poolSizes = {
            { RHI::DescriptorType::CombinedImageSampler, 2000 },
            { RHI::DescriptorType::UniformBuffer,        2000 },
        };
        poolDesc.freeDescriptorSet = true;
        poolDesc.debugName = "ImGuiDescriptorPool";

        m_descriptorPool = resMgr->createDescriptorPool(poolDesc);
    }

    // ════════════════════════════════════════
    // 每帧
    // ════════════════════════════════════════

    void ImGuiManager::beginFrame() {
        if (!m_glfwInitialized || !m_vulkanBackendReady) return;

        ImGui::SetCurrentContext(m_context);
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiManager::endFrame() {
        if (!m_glfwInitialized || !m_vulkanBackendReady) return;

        ImGui::SetCurrentContext(m_context);
        ImGui::Render();
    }

    // ════════════════════════════════════════
    // 渲染
    // ════════════════════════════════════════

    void ImGuiManager::render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) {
        if (!m_vulkanBackendReady) return;

        ImGui::SetCurrentContext(m_context);
        ImDrawData* drawData = ImGui::GetDrawData();
        if (!drawData || drawData->CmdListsCount == 0) return;

        VkCommandBuffer cmd = static_cast<VkCommandBuffer>(encoder->getCommandBuffer());
        ImGui_ImplVulkan_RenderDrawData(drawData, cmd);
    }

    // ════════════════════════════════════════
    // 事件转发 — 使用 ImGui 官方的 GLFW 回调
    // ════════════════════════════════════════

    void ImGuiManager::onKeyEvent(int glfwKey, int scancode, int action, int mods) {
        if (!m_glfwInitialized || !m_window) return;
        ImGui::SetCurrentContext(m_context);
        ImGui_ImplGlfw_KeyCallback(m_window->getHandle(), glfwKey, scancode, action, mods);
    }

    void ImGuiManager::onMouseButtonEvent(int button, int action, int mods) {
        if (!m_glfwInitialized || !m_window) return;
        ImGui::SetCurrentContext(m_context);
        ImGui_ImplGlfw_MouseButtonCallback(m_window->getHandle(), button, action, mods);
    }

    void ImGuiManager::onMouseMoveEvent(float x, float y) {
        if (!m_glfwInitialized) return;
        ImGui::SetCurrentContext(m_context);
        ImGui_ImplGlfw_CursorPosCallback(m_window->getHandle(), x, y);
    }

    void ImGuiManager::onMouseScrollEvent(float xOffset, float yOffset) {
        if (!m_glfwInitialized || !m_window) return;
        ImGui::SetCurrentContext(m_context);
        ImGui_ImplGlfw_ScrollCallback(m_window->getHandle(), xOffset, yOffset);
    }

    void ImGuiManager::onWindowResize(uint32_t width, uint32_t height) {
        m_width = width;
        m_height = height;
    }

    // ════════════════════════════════════════
    // 样式
    // ════════════════════════════════════════

    void ImGuiManager::setDarkTheme() {
        ImGui::StyleColorsDark();

        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 4.0f;
        style.FrameRounding = 3.0f;
        style.GrabRounding = 3.0f;
        style.ScrollbarRounding = 3.0f;
        style.TabRounding = 3.0f;

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
        colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.14f, 0.17f, 1.00f);
    }

    void ImGuiManager::setDefaultFont(const std::string& fontPath) {
        ImGuiIO& io = ImGui::GetIO();
        if (!fontPath.empty()) {
            io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 18.0f);  // ← 18px
        }
        else {
            ImFontConfig config;
            config.SizePixels = 18.0f;               
            io.Fonts->AddFontDefault(&config);
        }
    }

    void ImGuiManager::registerSceneTexture(RHI::ResourceManager* resMgr) {
        if (!m_rdg || !m_vulkanBackendReady || !resMgr) return;

        // 如果已有，先移除
        if (m_sceneTextureID != 0) {
            ImGui_ImplVulkan_RemoveTexture(reinterpret_cast<VkDescriptorSet>(m_sceneTextureID));
            m_sceneTextureID = 0;
        }

        // 获取 SceneColor 纹理
        auto texId = m_rdg->getTextureId("SceneColor");
        RHI::TextureHandle scHandle = m_rdg->getPhysicalTextureHandle(texId);
        if (!scHandle.isValid()) {
            LOG_ERROR("SceneColor texture not available");
            return;
        }
        auto* tex = resMgr->getTexture(scHandle);
        VkImageView view = static_cast<VkImageView>(tex->getDefaultView());

        // 获取采样器
        VkSampler sampler = VK_NULL_HANDLE;
        if (m_defaultSampler.isValid()) {
            sampler = static_cast<VkSampler>(resMgr->getSampler(m_defaultSampler)->getNativeHandle());
        }
        else {
            LOG_ERROR("Default sampler not set for ImGui scene texture");
            return;
        }

        // 注册纹理，并将指针转为 ImTextureID
        VkDescriptorSet descSet = ImGui_ImplVulkan_AddTexture(sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        m_sceneTextureID = reinterpret_cast<ImTextureID>(descSet);
    }
} // namespace StarryEngine