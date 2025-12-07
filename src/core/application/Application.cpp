// Application.cpp - 完整修改版
#include "Application.hpp"
#include "../../renderer/resourceManager/models/geometry/shape/Cube.hpp"
#include "../../renderer/resourceManager/shaders/ShaderBuilder.hpp"
#include "../../renderer/backends/vulkan/pipeline/NewPipelineBuilder.hpp"
#include "../../renderer/resourceManager/buffers/UniformBuffer.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <stdexcept>
#include <array>
#include <iostream>

// 包含所有组件类
#include "../../renderer/backends/vulkan/pipeline/pipelineStateComponent/ColorBlendComponent.hpp"
#include "../../renderer/backends/vulkan/pipeline/pipelineStateComponent/DepthStencilComponent.hpp"
#include "../../renderer/backends/vulkan/pipeline/pipelineStateComponent/DynamicStateComponent.hpp"
#include "../../renderer/backends/vulkan/pipeline/pipelineStateComponent/InputAssemblyComponent.hpp"
#include "../../renderer/backends/vulkan/pipeline/pipelineStateComponent/MultiSampleComponent.hpp"
#include "../../renderer/backends/vulkan/pipeline/pipelineStateComponent/RasterizationComponent.hpp"
#include "../../renderer/backends/vulkan/pipeline/pipelineStateComponent/ShaderStageComponent.hpp"
#include "../../renderer/backends/vulkan/pipeline/pipelineStateComponent/VertexInputComponent.hpp"
#include "../../renderer/backends/vulkan/pipeline/pipelineStateComponent/ViewportComponent.hpp"

namespace StarryEngine {

    Application::Application() {
        try {
            initialize();
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to initialize application: " << e.what() << std::endl;
            throw;
        }
    }

    void Application::initialize() {
        // 1. 创建窗口
        Window::Config windowConfig{
            .width = mWidth,
            .height = mHeight,
            .title = "StarryEngine Component Pipeline Test",
            .resizable = true,
            .monitorIndex = 0,
            .fullScreen = false,
            .highDPI = false,
            .iconPath = "assets/icons/window_icon.png"
        };
        mWindow = Window::create(windowConfig);

        // 设置窗口回调
        mWindow->setKeyCallback([this](int key, int action) {
            if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
                glfwSetWindowShouldClose(mWindow->getHandle(), GLFW_TRUE);
            }
            });

        mWindow->setResizeCallback([this](int width, int height) {
            mFramebufferResized = true;
            mWidth = width;
            mHeight = height;
            });

        // 2. 初始化Vulkan核心
        mVulkanCore = VulkanCore::create();
        mVulkanCore->init(mWindow);

        // 3. 创建命令池和窗口上下文
        auto commandPool = CommandPool::create(mVulkanCore->getLogicalDevice());
        mWindowContext = WindowContext::create();
        mWindowContext->init(mVulkanCore, mWindow, commandPool);

        // 4. 初始化Vulkan后端
        if (!mVulkanBackend.initialize(mVulkanCore, mWindowContext)) {
            throw std::runtime_error("Failed to initialize Vulkan backend");
        }

        // 5. 创建组件注册表
        mComponentRegistry = std::make_shared<ComponentRegistry>();

        // 6. 注册默认组件
        registerDefaultComponents();

        // 7. 创建网格
        auto cube = Cube::create();
        auto geometry = cube->generateGeometry();
        mMesh = Mesh(geometry, mVulkanCore->getLogicalDevice(), mWindowContext->getCommandPool());

        // 8. 创建着色器程序
        createShaderProgram();

        // 9. 创建渲染通道
        createRenderPass();

        // 10. 创建深度纹理
        createDepthTexture();

        // 11. 创建Uniform Buffers
        createUniformBuffers();

        // 12. 创建描述符管理器
        createDescriptorManager();

        // 13. 创建图形管线
        createGraphicsPipeline();

        // 14. 创建帧缓冲
        createFramebuffers();
    }

    void Application::registerDefaultComponents() {
        auto logicalDevice = mVulkanCore->getLogicalDevice();

        // 1. 着色器阶段组件（稍后配置具体的着色器）
        auto shaderComponent = std::make_shared<ShaderStageComponent>("BasicShader");
        mComponentRegistry->registerComponent("BasicShader", shaderComponent);
        mComponentRegistry->setDefaultComponent(PipelineComponentType::SHADER_STAGE, "BasicShader");

        // 2. 顶点输入组件
        auto basicVertexInput = std::make_shared<VertexInputComponent>("BasicVertex");
        mComponentRegistry->registerComponent("BasicVertex", basicVertexInput);
        mComponentRegistry->setDefaultComponent(PipelineComponentType::VERTEX_INPUT, "BasicVertex");

        // 3. 输入装配组件
        auto triangleList = std::make_shared<InputAssemblyComponent>("TriangleList");
        triangleList->setTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .enablePrimitiveRestart(VK_FALSE);
        mComponentRegistry->registerComponent("TriangleList", triangleList);

        mComponentRegistry->setDefaultComponent(PipelineComponentType::INPUT_ASSEMBLY, "TriangleList");

        // 4. 视口状态组件
        auto fullscreenViewport = std::make_shared<ViewportComponent>("Fullscreen");
        mComponentRegistry->registerComponent("Fullscreen", fullscreenViewport);
        mComponentRegistry->setDefaultComponent(PipelineComponentType::VIEWPORT_STATE, "Fullscreen");

        // 5. 光栅化组件
        auto opaqueRasterization = std::make_shared<RasterizationComponent>("Opaque");
        opaqueRasterization->setCullMode(VK_CULL_MODE_NONE)
            .setFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .setPolygonMode(VK_POLYGON_MODE_FILL)
            .setLineWidth(1.0f);
        mComponentRegistry->registerComponent("Opaque", opaqueRasterization);

        auto wireframeRasterization = std::make_shared<RasterizationComponent>("Wireframe");
        wireframeRasterization->setPolygonMode(VK_POLYGON_MODE_LINE)
            .setLineWidth(1.5f);
        mComponentRegistry->registerComponent("Wireframe", wireframeRasterization);

        mComponentRegistry->setDefaultComponent(PipelineComponentType::RASTERIZATION, "Opaque");

        // 6. 多重采样组件
        auto defaultMultisample = std::make_shared<MultiSampleComponent>("Default");
        defaultMultisample->enableSampleShading(VK_FALSE);
        mComponentRegistry->registerComponent("Default", defaultMultisample);
        mComponentRegistry->setDefaultComponent(PipelineComponentType::MULTISAMPLE, "Default");

        // 7. 深度模板组件
        auto depthEnabled = std::make_shared<DepthStencilComponent>("Enabled");
        depthEnabled->enableDepthTest(VK_TRUE)
            .enableDepthWrite(VK_TRUE)
            .setDepthCompareOp(VK_COMPARE_OP_LESS);
        mComponentRegistry->registerComponent("Enabled", depthEnabled);

        auto depthTestOnly = std::make_shared<DepthStencilComponent>("TestOnly");
        depthTestOnly->enableDepthTest(VK_TRUE)
            .enableDepthWrite(VK_FALSE)
            .setDepthCompareOp(VK_COMPARE_OP_LESS);
        mComponentRegistry->registerComponent("TestOnly", depthTestOnly);

        mComponentRegistry->setDefaultComponent(PipelineComponentType::DEPTH_STENCIL, "Enabled");

        // 8. 颜色混合组件
        auto noBlend = std::make_shared<ColorBlendComponent>("None");
        noBlend->addNoBlendingAttachment();
        mComponentRegistry->registerComponent("None", noBlend);

        auto alphaBlend = std::make_shared<ColorBlendComponent>("Alpha");
        alphaBlend->addAlphaBlendingAttachment();
        mComponentRegistry->registerComponent("Alpha", alphaBlend);

        mComponentRegistry->setDefaultComponent(PipelineComponentType::COLOR_BLEND, "None");

        // 9. 动态状态组件
        auto basicDynamic = std::make_shared<DynamicStateComponent>("Basic");
        basicDynamic->addViewportScissorStates();
        mComponentRegistry->registerComponent("Basic", basicDynamic);

        auto noDynamic = std::make_shared<DynamicStateComponent>("None");
        mComponentRegistry->registerComponent("None", noDynamic);

        mComponentRegistry->setDefaultComponent(PipelineComponentType::DYNAMIC_STATE, "Basic");
    }

    void Application::createShaderProgram() {
        mShaderProgram = ShaderProgram::create(mVulkanCore->getLogicalDevice());

        // 使用 ShaderBuilder 构建顶点着色器
        ShaderBuilder vertBuilder(ShaderType::Vertex, "#version 450");

        // 添加 uniform buffer
        vertBuilder.addUniformBuffer("UniformBufferObject", 0, 0,
            { "mat4 model", "mat4 view", "mat4 proj" });

        // 添加输入
        vertBuilder.addInput("vec3", "inPosition", 0);

        // 添加输出
        vertBuilder.addOutput("vec3", "fragColor", 0);

        // 设置 main 函数体
        vertBuilder.setMainBody(
            R"(gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
            fragColor = vec3(1.0,0.0,1.0);)"
        );

        std::string vertexShader = vertBuilder.getSource();
        std::cout << "=== Vertex Shader ===\n" << vertexShader << "\n\n";
        mShaderProgram->addGLSLStringStage(vertexShader, VK_SHADER_STAGE_VERTEX_BIT, "main", {}, "VertexShader");

        // 使用 ShaderBuilder 构建片段着色器
        ShaderBuilder fragBuilder(ShaderType::Fragment, "#version 450");

        // 添加输入（与顶点着色器的输出匹配）
        fragBuilder.addInput("vec3", "fragColor", 0);

        // 添加输出
        fragBuilder.addOutput("vec4", "outColor", 0);

        // 设置 main 函数体
        fragBuilder.setMainBody(R"(outColor = vec4(fragColor, 1.0);)");

        std::string fragmentShader = fragBuilder.getSource();
        std::cout << "=== Fragment Shader ===\n" << fragmentShader << "\n";
        mShaderProgram->addGLSLStringStage(fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT, "main", {}, "FragmentShader");

        // 注册一个自定义着色器组件
        auto customShaderComponent = std::make_shared<ShaderStageComponent>("CustomShader");
        customShaderComponent->setShaderProgram(mShaderProgram);
        mComponentRegistry->registerComponent("CustomShader", customShaderComponent);
    }

    void Application::createRenderPass() {
        // 使用RenderPassBuilder构建渲染通道
        RenderPassBuilder builder("MainRenderPass", mVulkanCore->getLogicalDevice());

        // 添加颜色附件
        builder.addColorAttachment(
            "ColorAttachment",
            mWindowContext->getSwapchainFormat(),
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE
        );

        // 添加深度附件
        builder.addDepthAttachment(
            "DepthAttachment",
            Texture::findSupportedDepthFormat(mVulkanCore->getPhysicalDeviceHandle()),
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_DONT_CARE
        );

        // 创建并配置子流程
        SubpassBuilder subpassBuilder("GeometryPass");
        subpassBuilder.addColorAttachment("ColorAttachment");
        subpassBuilder.setDepthStencilAttachment("DepthAttachment");
        subpassBuilder.setPipelineName("MainPipeline");

        // 添加到渲染通道构建器
        builder.addSubpass(subpassBuilder);

        // 构建渲染通道
        mRenderPassResult = builder.build(true);
        if (!mRenderPassResult) {
            throw std::runtime_error("Failed to build render pass");
        }

        mRenderPass = mRenderPassResult->renderPass->getHandle();
    }

    void Application::createDepthTexture() {
        // 使用Texture类创建深度纹理
        auto extent = mWindowContext->getSwapchainExtent();
        mDepthTexture = Texture::create(
            mVulkanCore->getLogicalDevice(),
            Texture::Type::Depth,
            extent,
            mWindowContext->getCommandPool()
        );
    }

    void Application::createDescriptorManager() {
        mDescriptorManager = std::make_shared<DescriptorManager>(mVulkanCore->getLogicalDevice());

        // 定义描述符集布局
        mDescriptorManager->beginSetLayout(0);
        mDescriptorManager->addUniformBuffer(0, VK_SHADER_STAGE_VERTEX_BIT, 1);
        mDescriptorManager->endSetLayout();

        // 分配描述符集
        mDescriptorManager->allocateSets(MAX_FRAMES_IN_FLIGHT);

        // 为每个Uniform Buffer更新描述符集
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            mDescriptorManager->updateUniformBuffer(
                0, 0, i,
                mUniformBuffers[i]->getBuffer(),
                0, sizeof(UniformBufferObject)
            );
        }
    }

    void Application::createUniformBuffers() {
        mUniformBuffers.clear();

        // 为每帧创建一个UniformBuffer
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            auto uniformBuffer = UniformBuffer::create(
                mVulkanCore->getLogicalDevice(),
                mWindowContext->getCommandPool(),
                sizeof(UniformBufferObject)
            );
            mUniformBuffers.push_back(uniformBuffer);
        }
    }

    void Application::createGraphicsPipeline() {
        try {
            // 1. 创建NewPipelineBuilder
            mPipelineBuilder = std::make_shared<PipelineBuilder>(
                mVulkanCore->getLogicalDeviceHandle(),
                mComponentRegistry
            );

            std::cout << "Created PipelineBuilder successfully" << std::endl;

            // 2. 配置顶点输入组件（基于网格的实际顶点数据）
            auto vertexInputComponent = std::dynamic_pointer_cast<VertexInputComponent>(
                mComponentRegistry->getComponent(PipelineComponentType::VERTEX_INPUT, "BasicVertex"));

            if (vertexInputComponent) {
                std::cout << "Configuring vertex input component..." << std::endl;
                auto bindingDescriptions = mMesh.getVertexBuffer()->getBindingDescriptions();
                auto attributeDescriptions = mMesh.getVertexBuffer()->getAttributeDescriptions();

                std::cout << "Found " << bindingDescriptions.size() << " binding descriptions" << std::endl;
                std::cout << "Found " << attributeDescriptions.size() << " attribute descriptions" << std::endl;

                vertexInputComponent->reset();

                for (const auto& binding : bindingDescriptions) {
                    std::cout << "Adding binding: binding=" << binding.binding
                        << ", stride=" << binding.stride
                        << ", inputRate=" << binding.inputRate << std::endl;
                    vertexInputComponent->addBinding(binding.binding, binding.stride, binding.inputRate);
                }

                for (const auto& attr : attributeDescriptions) {
                    std::cout << "Adding attribute: location=" << attr.location
                        << ", binding=" << attr.binding
                        << ", format=" << attr.format
                        << ", offset=" << attr.offset << std::endl;
                    vertexInputComponent->addAttribute(attr.location, attr.binding, attr.format, attr.offset);
                }

                if (!vertexInputComponent->isValid()) {
                    throw std::runtime_error("Vertex input component is invalid");
                }
                std::cout << "Vertex input component is valid" << std::endl;
            }
            else {
                throw std::runtime_error("Failed to get vertex input component");
            }

            // 3. 配置视口状态组件（基于交换链尺寸）
            auto viewportComponent = std::dynamic_pointer_cast<ViewportComponent>(
                mComponentRegistry->getComponent(PipelineComponentType::VIEWPORT_STATE, "Fullscreen"));

            if (viewportComponent) {
                auto swapchainExtent = mWindowContext->getSwapchainExtent();
                std::cout << "Swapchain extent: " << swapchainExtent.width << "x" << swapchainExtent.height << std::endl;

                viewportComponent->reset();

                VkViewport viewport = {
                    0.0f,
                    0.0f,
                    static_cast<float>(swapchainExtent.width),
                    static_cast<float>(swapchainExtent.height),
                    0.0f,
                    1.0f
                };
                viewportComponent->addViewport(viewport);

                VkRect2D scissor = {
                    {0, 0},
                    swapchainExtent
                };
                viewportComponent->addScissor(scissor);

                std::cout << "Viewport component configured" << std::endl;
            }
            else {
                throw std::runtime_error("Failed to get viewport component");
            }

            // 4. 获取描述符集布局
            auto descriptorSetLayouts = mDescriptorManager->getLayoutHandles();
            std::cout << "Descriptor set layouts count: " << descriptorSetLayouts.size() << std::endl;

            // 5. 创建管线布局
            std::cout << "Creating pipeline layout..." << std::endl;
            mPipelineLayout = PipelineLayout::create(
                mVulkanCore->getLogicalDevice(),
                descriptorSetLayouts
            );

            if (!mPipelineLayout) {
                throw std::runtime_error("Failed to create pipeline layout");
            }
            std::cout << "Pipeline layout created" << std::endl;

            // 6. 验证组件
            std::cout << "Validating component selections..." << std::endl;

            // 先检查所有组件是否都存在
            std::vector<std::pair<PipelineComponentType, std::string>> componentsToCheck = {
                {PipelineComponentType::SHADER_STAGE, "CustomShader"},
                {PipelineComponentType::VERTEX_INPUT, "BasicVertex"},
                {PipelineComponentType::INPUT_ASSEMBLY, "TriangleList"},
                {PipelineComponentType::VIEWPORT_STATE, "Fullscreen"},
                {PipelineComponentType::RASTERIZATION, "Opaque"},
                {PipelineComponentType::MULTISAMPLE, "Default"},
                {PipelineComponentType::DEPTH_STENCIL, "Enabled"},
                {PipelineComponentType::COLOR_BLEND, "None"},
                {PipelineComponentType::DYNAMIC_STATE, "Basic"}
            };

            for (const auto& [type, name] : componentsToCheck) {
                auto component = mComponentRegistry->getComponent(type, name);
                if (!component) {
                    std::cerr << "ERROR: Component not found: type=" << static_cast<int>(type)
                        << ", name=" << name << std::endl;
                }
                else {
                    std::cout << "Component found: type=" << static_cast<int>(type)
                        << ", name=" << name << std::endl;
                }
            }

            // 7. 使用组件构建管线
            std::cout << "Building graphics pipeline..." << std::endl;
            mGraphicsPipeline = mPipelineBuilder
                ->addComponent(PipelineComponentType::SHADER_STAGE, "CustomShader")
                .addComponent(PipelineComponentType::VERTEX_INPUT, "BasicVertex")
                .addComponent(PipelineComponentType::INPUT_ASSEMBLY, "TriangleList")
                .addComponent(PipelineComponentType::VIEWPORT_STATE, "Fullscreen")
                .addComponent(PipelineComponentType::RASTERIZATION, "Opaque")
                .addComponent(PipelineComponentType::MULTISAMPLE, "Default")
                .addComponent(PipelineComponentType::DEPTH_STENCIL, "Enabled")
                .addComponent(PipelineComponentType::COLOR_BLEND, "None")
                .addComponent(PipelineComponentType::DYNAMIC_STATE, "Basic")
                .buildGraphicsPipeline(
                    mPipelineLayout->getHandle(),
                    mRenderPass,
                    0  // subpass
                );

            if (mGraphicsPipeline == VK_NULL_HANDLE) {
                throw std::runtime_error("Graphics pipeline creation returned VK_NULL_HANDLE");
            }

            std::cout << "Graphics pipeline created successfully using component system" << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "ERROR in createGraphicsPipeline: " << e.what() << std::endl;
            throw;  // 重新抛出异常
        }
    }

    void Application::createFramebuffers() {
        auto swapchain = mWindowContext->getSwapChain();
        auto imageViews = swapchain->getImageViews();
        mSwapchainFramebuffers.resize(imageViews.size());

        auto device = mVulkanCore->getLogicalDeviceHandle();
        auto extent = mWindowContext->getSwapchainExtent();

        for (size_t i = 0; i < imageViews.size(); i++) {
            std::vector<VkImageView> attachments = {
                imageViews[i],
                mDepthTexture->getImageView()
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = mRenderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = extent.width;
            framebufferInfo.height = extent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &mSwapchainFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create framebuffer!");
            }
        }
    }

    void Application::updateUniformBuffer(uint32_t currentFrame) {
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float>(currentTime - startTime).count();

        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view = glm::lookAt(
            glm::vec3(2.0f, 2.0f, 2.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f)
        );
        ubo.proj = glm::perspective(
            glm::radians(45.0f),
            mWindow->getAspectRatio(),
            0.1f,
            10.0f
        );
        ubo.proj[1][1] *= -1; // 翻转Y轴

        // 使用UniformBuffer的updateData方法
        mUniformBuffers[currentFrame]->updateData(&ubo, sizeof(ubo));
    }

    void Application::recordCommandBuffer(RenderContext& context, uint32_t imageIndex) {
        // 开始渲染通道
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = mRenderPass;
        renderPassInfo.framebuffer = mSwapchainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = mWindowContext->getSwapchainExtent();

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
        clearValues[1].depthStencil = { 1.0f, 0 };

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        context.beginRenderPass(&renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // 动态设置视口
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(mWindowContext->getSwapchainExtent().width);
        viewport.height = static_cast<float>(mWindowContext->getSwapchainExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        context.setViewport(viewport);

        // 动态设置裁剪
        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = mWindowContext->getSwapchainExtent();
        context.setScissor(scissor);

        // 绑定管线
        context.bindGraphicsPipeline(mGraphicsPipeline);

        // 绑定顶点缓冲区
        auto vertexBuffers = mMesh.getVertexBuffer()->getBufferHandles();
        context.bindVertexBuffers(vertexBuffers);

        // 绑定索引缓冲区
        context.bindIndexBuffer(mMesh.getIndexBuffer()->getBuffer());

        // 绑定描述符集
        uint32_t frameIndex = mVulkanBackend.getCurrentFrameIndex();
        VkDescriptorSet descriptorSet = mDescriptorManager->getDescriptorSet(0, frameIndex);
        context.bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, descriptorSet, 0, mPipelineLayout->getHandle());

        // 绘制
        context.drawIndexed(mMesh.getIndexBuffer()->getIndexCount(), 1, 0, 0, 0);

        // 结束渲染通道
        context.endRenderPass();
    }

    void Application::drawFrame() {
        mVulkanBackend.beginFrame();
        uint32_t frameIndex = mVulkanBackend.getCurrentFrameIndex();
        uint32_t imageIndex = mVulkanBackend.getCurrentImageIndex();
        updateUniformBuffer(frameIndex);
        if (auto* ctx = mVulkanBackend.getCurrentFrameContext()) {
            recordCommandBuffer(*ctx->renderContext, imageIndex);
        }
        mVulkanBackend.submitFrame();
    }

    void Application::run() {
        while (!glfwWindowShouldClose(mWindow->getHandle())) {
            glfwPollEvents();

            // 绘制帧
            try {
                drawFrame();
            }
            catch (...) {
                recreateSwapchain();
                continue;
            }

            // 检查是否需要重建交换链
            if (mFramebufferResized) {
                mFramebufferResized = false;
                recreateSwapchain();
            }
        }

        // 等待设备空闲
        vkDeviceWaitIdle(mVulkanCore->getLogicalDeviceHandle());
    }

    void Application::cleanupSwapchain() {
        auto device = mVulkanCore->getLogicalDeviceHandle();

        // 清理帧缓冲
        for (auto framebuffer : mSwapchainFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
        mSwapchainFramebuffers.clear();
    }

    void Application::recreateSwapchain() {
        // 等待设备空闲
        vkDeviceWaitIdle(mVulkanCore->getLogicalDeviceHandle());

        // 清理旧交换链资源
        cleanupSwapchain();

        // 重建交换链
        mWindowContext->recreateSwapchain();

        // 重新创建深度纹理（尺寸可能已改变）
        createDepthTexture();

        // 重新配置视口组件
        auto viewportComponent = std::dynamic_pointer_cast<ViewportComponent>(
            mComponentRegistry->getComponent(PipelineComponentType::VIEWPORT_STATE, "Fullscreen"));

        if (viewportComponent) {
            auto swapchainExtent = mWindowContext->getSwapchainExtent();
            viewportComponent->reset();

            // 创建 VkViewport 对象
            VkViewport viewport = {
                0.0f,  // x
                0.0f,  // y
                static_cast<float>(swapchainExtent.width),  // width
                static_cast<float>(swapchainExtent.height), // height
                0.0f,  // minDepth
                1.0f   // maxDepth
            };
            viewportComponent->addViewport(viewport);

            // 创建 VkRect2D 对象
            VkRect2D scissor = {
                {0, 0},  // offset
                swapchainExtent  // extent
            };
            viewportComponent->addScissor(scissor);
        }

        // 重新创建帧缓冲
        createFramebuffers();
    }

    void Application::cleanup() {
        auto device = mVulkanCore->getLogicalDeviceHandle();

        // 清理交换链资源
        cleanupSwapchain();

        // 清理图形管线
        if (mGraphicsPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, mGraphicsPipeline, nullptr);
            mGraphicsPipeline = VK_NULL_HANDLE;
        }

        // 清理管线布局
        if (mPipelineLayout) {
            mPipelineLayout.reset();
        }

        // 清理Uniform Buffers
        mUniformBuffers.clear();

        // 清理深度纹理
        if (mDepthTexture) {
            mDepthTexture->cleanup();
        }

        // 清理描述符管理器
        if (mDescriptorManager) {
            mDescriptorManager->cleanup();
        }

        // 清理着色器程序
        if (mShaderProgram) {
            // ShaderProgram有析构函数会自动清理
        }

        // 清理组件注册表
        if (mComponentRegistry) {
            mComponentRegistry->clear();
        }

        // 清理管线构建器
        mPipelineBuilder.reset();

        // 清理渲染通道
        mRenderPassResult.reset();

        // 清理后端
        mVulkanBackend.shutdown();

        // 清理窗口上下文和Vulkan核心
        mWindowContext.reset();
        mVulkanCore.reset();
        mWindow.reset();
    }

    Application::~Application() {
        cleanup();
    }

} // namespace StarryEngine