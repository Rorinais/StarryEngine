#include "Application.hpp"
#include "../../renderer/resourceManager/models/geometry/shape/Cube.hpp"
#include "../../renderer/resourceManager/shaders/ShaderBuilder.hpp"
#include "../../renderer/backends/vulkan/pipeline/PipelineBuilder.hpp"
#include "../../renderer/resourceManager/buffers/UniformBuffer.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <stdexcept>
#include <array>
#include <iostream>

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
            .title = "StarryEngine RenderPass Test",
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

        // 5. 创建网格
        auto cube = Cube::create();
        auto geometry = cube->generateGeometry();
        mMesh = Mesh(geometry, mVulkanCore->getLogicalDevice(), mWindowContext->getCommandPool());

        // 6. 创建着色器程序
        createShaderProgram();

        // 7. 创建渲染通道
        createRenderPass();

        // 8. 创建深度纹理
        createDepthTexture();

        // 9. 创建Uniform Buffers
        createUniformBuffers();

        // 10. 创建描述符管理器
        createDescriptorManager();

        // 11. 创建图形管线
        createGraphicsPipeline();

        // 12. 创建帧缓冲
        createFramebuffers();

        // 13. 初始化额外的同步对象
        mImagesInFlight.resize(mWindowContext->getSwapchainImageCount(), VK_NULL_HANDLE);
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
        // 1. 配置顶点输入状态
        VertexInput vertexInput;

        // 从Mesh获取绑定描述
        auto bindingDescriptions = mMesh.getVertexBuffer()->getBindingDescriptions();
        auto attributeDescriptions = mMesh.getVertexBuffer()->getAttributeDescriptions();

        for (const auto& binding : bindingDescriptions) {
            vertexInput.addBinding(binding.binding, binding.stride);
        }

        for (const auto& attr : attributeDescriptions) {
            vertexInput.addAttribute(attr.binding, attr);
        }

        // 2. 配置视口状态
        auto swapchainExtent = mWindowContext->getSwapchainExtent();
        auto viewport = Viewport().init(swapchainExtent).IsOpenglCoordinate(false);

        // 3. 配置光栅化状态
        auto rasterization = Rasterization()
            .setCullMode(VK_CULL_MODE_BACK_BIT)
            .setFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .setPolygonMode(VK_POLYGON_MODE_FILL)
            .setLineWidth(1.0f);

        // 4. 配置深度模板状态
        auto depthStencil = DepthStencil()
            .enableDepthTest(VK_TRUE)
            .enableDepthWrite(VK_TRUE)
            .setDepthCompareOp(VK_COMPARE_OP_LESS);

        // 5. 配置颜色混合状态
        auto colorBlend = ColorBlend().addAttachment(
            ColorBlend::Config{
                VK_FALSE,
                VK_BLEND_FACTOR_ONE,
                VK_BLEND_FACTOR_ZERO,
                VK_BLEND_OP_ADD,
                VK_BLEND_FACTOR_ONE,
                VK_BLEND_FACTOR_ZERO,
                VK_BLEND_OP_ADD,
                VK_COLOR_COMPONENT_R_BIT |
                VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT
            }
        );

        // 6. 配置输入装配状态
        auto inputAssembly = InputAssembly()
            .setTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .enablePrimitiveRestart(VK_FALSE);

        // 7. 配置多重采样状态
        auto multiSample = MultiSample()
            .enableSampleShading(VK_FALSE)
            .setSampleCount(VK_SAMPLE_COUNT_1_BIT);

        // 8. 配置动态状态
        auto dynamic = Dynamic().setEnableDynamic(true);

        // 9. 获取描述符集布局
        auto descriptorSetLayouts = mDescriptorManager->getLayoutHandles();

        // 10. 创建PipelineBuilder并配置
        PipelineBuilder pipelineBuilder(mRenderPass, mVulkanCore->getLogicalDevice());

        mGraphicsPipeline = pipelineBuilder
            .setShaderProgram(mShaderProgram)
            .setVertexInput(vertexInput)
            .setInputAssembly(inputAssembly)
            .setViewport(viewport)
            .setRasterization(rasterization)
            .setMultisample(multiSample)
            .setDepthStencil(depthStencil)
            .setColorBlend(colorBlend)
            .setDynamic(dynamic)
            .setDescriptorSetLayouts(descriptorSetLayouts)
            .build();

        if (!mGraphicsPipeline) {
            throw std::runtime_error("Failed to create graphics pipeline");
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
                mDepthTexture->getImageView()  // 使用深度纹理的图像视图
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
        //ubo.proj[1][1] *= -1; // 翻转Y轴

        // 使用UniformBuffer的uploadData方法
        mUniformBuffers[currentFrame]->uploadData(&ubo, sizeof(ubo));
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

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(mWindowContext->getSwapchainExtent().width);
        viewport.height = static_cast<float>(mWindowContext->getSwapchainExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        context.setViewport(viewport); // 假设你的RenderContext封装了vkCmdSetViewport

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = mWindowContext->getSwapchainExtent();
        context.setScissor(scissor);

        // 绑定管线
        context.bindGraphicsPipeline(mGraphicsPipeline->getHandle());

        // 绑定顶点缓冲区
        auto vertexBuffers = mMesh.getVertexBuffer()->getBufferHandles();
        context.bindVertexBuffers(vertexBuffers);

        // 绑定索引缓冲区
        context.bindIndexBuffer(mMesh.getIndexBuffer()->getBuffer());

        // 绑定描述符集
        uint32_t frameIndex = mVulkanBackend.getCurrentFrameIndex();
        VkDescriptorSet descriptorSet = mDescriptorManager->getDescriptorSet(0, frameIndex);
        VkPipelineLayout pipelineLayout = mGraphicsPipeline->getPipelineLayout()->getHandle();
        context.bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, descriptorSet, 0, pipelineLayout);

        // 绘制
        context.drawIndexed(mMesh.getIndexBuffer()->getIndexCount(), 1, 0, 0, 0);

        // 结束渲染通道
        context.endRenderPass();
    }

    void Application::drawFrame() {
        mVulkanBackend.beginFrame();
        uint32_t frameIndex = mVulkanBackend.getCurrentFrameIndex();
        uint32_t imageIndex = mVulkanBackend.getCurrentImageIndex(); // 使用正确索引
        updateUniformBuffer(frameIndex);
        if (auto* ctx = mVulkanBackend.getCurrentFrameContext()) {
            recordCommandBuffer(*ctx->renderContext, imageIndex); // 传递正确索引
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

        // 重新创建帧缓冲
        createFramebuffers();
    }

    void Application::cleanup() {
        auto device = mVulkanCore->getLogicalDeviceHandle();

        // 清理交换链资源
        cleanupSwapchain();

        // 清理Uniform Buffers（自动管理）
        mUniformBuffers.clear();

        // 清理深度纹理
        if (mDepthTexture) {
            mDepthTexture->cleanup();
        }

        // 清理管线
        if (mGraphicsPipeline) {
            mGraphicsPipeline->cleanup();
        }

        // 清理描述符管理器
        if (mDescriptorManager) {
            mDescriptorManager->cleanup();
        }

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