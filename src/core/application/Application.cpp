#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <stdexcept>
#include <array>
#include <iostream>
#include "Application.hpp"
#include "../../renderer/resourceManager/models/geometry/shape/Cube.hpp"
#include "../../renderer/resourceManager/shaders/ShaderBuilder.hpp"
#include "../../renderer/backends/vulkan/pipeline/NewPipelineBuilder.hpp"
#include "../../renderer/resourceManager/buffers/UniformBuffer.hpp"
#include "../../renderer/resourceManager/models/ModelLoader.hpp"

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

        mVulkanCore = VulkanCore::create();
        mVulkanCore->init(mWindow);

        auto commandPool = CommandPool::create(mVulkanCore->getLogicalDevice());
        mWindowContext = WindowContext::create();
        mWindowContext->init(mVulkanCore, mWindow, commandPool);

        if (!mVulkanBackend.initialize(mVulkanCore, mWindowContext)) {
            throw std::runtime_error("Failed to initialize Vulkan backend");
        }

        registerDefaultComponents();

        auto cube = Cube::create();
        auto geometry = cube->generateGeometry();
        mMesh = Mesh(geometry, mVulkanCore->getLogicalDevice(), mWindowContext->getCommandPool());

        loader =std::make_shared<ModelLoader>(mVulkanCore, mWindowContext->getCommandPool()); 
        loader->loadMesh("./assets/models/Griseo.fbx");
        loader->generateBuffer();

        createShaderProgram();
        createRenderPass();
        createDepthTexture();
        createDescriptorManager();
        createGraphicsPipeline();
        createFramebuffers();
    }

    void Application::registerDefaultComponents() {
        auto logicalDevice = mVulkanCore->getLogicalDevice();
        mComponentRegistry = std::make_shared<ComponentRegistry>();

        // 1. 着色器阶段组件
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

        mShaderProgram->addGLSLStage(
            "assets/shaders/core/shader.vert",
            VK_SHADER_STAGE_VERTEX_BIT,
            "main",
            {},
            "VertexShader"
		);

        mShaderProgram->addGLSLStage(
            "assets/shaders/core/shader.frag",
            VK_SHADER_STAGE_FRAGMENT_BIT,
            "main",
            {},
            "FragmentShader"
        );

        auto customShaderComponent = std::make_shared<ShaderStageComponent>("CustomShader");
        customShaderComponent->setShaderProgram(mShaderProgram);
        mComponentRegistry->registerComponent("CustomShader", customShaderComponent);
    }

    void Application::createRenderPass() {
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
        mMatrixUniformBuffers.clear();
        mColorUniformBuffers.clear();

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            auto matrixUniformBuffer = UniformBuffer::create<UniformBufferObject>(
                mVulkanCore->getLogicalDevice(),mWindowContext->getCommandPool()
            );
            mMatrixUniformBuffers.push_back(matrixUniformBuffer);

            // auto colorUniformBuffer = UniformBuffer::create<glm::vec3>(
            //     mVulkanCore->getLogicalDevice(),mWindowContext->getCommandPool()
            // );
            // mColorUniformBuffers.push_back(colorUniformBuffer);
        }

        mDescriptorManager = std::make_shared<DescriptorManager>(mVulkanCore->getLogicalDevice());

        // 定义描述符集布局 - set 0有两个binding
        mDescriptorManager->beginSetLayout(0);
        mDescriptorManager->addUniformBuffer(0, VK_SHADER_STAGE_VERTEX_BIT, 1);    
        mDescriptorManager->addUniformBuffer(1, VK_SHADER_STAGE_FRAGMENT_BIT, 1); 
        mDescriptorManager->endSetLayout();

        // 分配描述符集
        mDescriptorManager->allocateSets(MAX_FRAMES_IN_FLIGHT);

        // 为每个帧更新描述符集
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            mDescriptorManager->writeUniformBufferDescriptor<UniformBufferObject>(
                0, 0, i, mMatrixUniformBuffers[i]->getBuffer()
            );

            // mDescriptorManager->writeUniformBufferDescriptor<glm::vec3>(
            //     0, 1, i, mColorUniformBuffers[i]->getBuffer()
            // );
        }
    }

    void Application::createGraphicsPipeline() {
        try {
            // 1. 创建NewPipelineBuilder
            mPipelineBuilder = std::make_shared<PipelineBuilder>(
                mVulkanCore->getLogicalDeviceHandle(),
                mComponentRegistry
            );

            // 2. 配置顶点输入组件（基于网格的实际顶点数据）
            auto vertexInputComponent = std::dynamic_pointer_cast<VertexInputComponent>(
                mComponentRegistry->getComponent(PipelineComponentType::VERTEX_INPUT, "BasicVertex"));

            if (vertexInputComponent) {
                // auto bindingDescriptions = mMesh.getVertexBuffer()->getBindingDescriptions();
                // auto attributeDescriptions = mMesh.getVertexBuffer()->getAttributeDescriptions();

                vertexInputComponent->reset();

                // for (const auto& binding : bindingDescriptions) {
                //     vertexInputComponent->addBinding(binding.binding, binding.stride, binding.inputRate);
                // }

                // for (const auto& attr : attributeDescriptions) {
                //     vertexInputComponent->addAttribute(attr.location, attr.binding, attr.format, attr.offset);
                // }
                vertexInputComponent->configureFromVertexBuffer(*loader->getVertexBuffers());

                if (!vertexInputComponent->isValid()) {
                    throw std::runtime_error("Vertex input component is invalid");
                }
            }
            else {
                throw std::runtime_error("Failed to get vertex input component");
            }

            // 3. 配置视口状态组件（基于交换链尺寸）
            auto viewportComponent = std::dynamic_pointer_cast<ViewportComponent>(
                mComponentRegistry->getComponent(PipelineComponentType::VIEWPORT_STATE, "Fullscreen"));

            if (viewportComponent) {
                viewportComponent->reset();

                ViewportScissor viewport(mWindowContext->getSwapchainExtent(),true);
                viewportComponent->setViewportScissor(viewport);
            }
            else {
                throw std::runtime_error("Failed to get viewport component");
            }

            // 4. 获取描述符集布局
            auto descriptorSetLayouts = mDescriptorManager->getLayoutHandles();

            // 5. 创建管线布局
            mPipelineLayout = PipelineLayout::create(
                mVulkanCore->getLogicalDevice(),
                descriptorSetLayouts
            );

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
                    mRenderPassResult->renderPass->getHandle(),
					mRenderPassResult->pipelineNameToSubpassIndexMap["MainPipeline"]
                );
        }
        catch (const std::exception& e) {
            std::cerr << "ERROR in createGraphicsPipeline: " << e.what() << std::endl;
            throw;  
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
            framebufferInfo.renderPass = mRenderPassResult->renderPass->getHandle();
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

        static glm::vec3 position = glm::vec3(0.0f, 0.0f, -0.3f);
        static glm::vec3 scale = glm::vec3(0.3f, 0.3f, 0.3f);

        // 更新矩阵
        UniformBufferObject ubo{};

        ubo.model = glm::mat4(1.0f);
        ubo.model = glm::translate(ubo.model, position);  // 平移
        ubo.model = glm::rotate(ubo.model, time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));  // 旋转
        ubo.model = glm::scale(ubo.model, scale);

        ubo.view = glm::lookAt(
            glm::vec3(1.0f, 1.0f, 1.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f)
        );
        ubo.proj = glm::perspective(
            glm::radians(15.0f),
            mWindow->getAspectRatio(),
            0.1f,
            10.0f
        );

        //视口已经反转，如果是vulkan坐标系，则要将投影矩阵[1][1]反转
        //步骤如下：
        //1.打开注释代码
        //2.将viewport(mWindowContext->getSwapchainExtent(),true)改成false

        //ubo.proj[1][1] *= -1; 

        mMatrixUniformBuffers[currentFrame]->updateData(&ubo, sizeof(ubo));

        // 更新颜色
        // glm::vec3 color = glm::vec3(
        //     (sin(time * 0.5f) + 1.0f) / 2.0f,
        //     (sin(time * 0.8f + glm::radians(120.0f)) + 1.0f) / 2.0f,
        //     (sin(time * 1.1f + glm::radians(240.0f)) + 1.0f) / 2.0f
        // );

        // mColorUniformBuffers[currentFrame]->updateData(&color, sizeof(color));
    }

    void Application::recordCommandBuffer(RenderContext& context, uint32_t imageIndex) {
        auto renderpass = mRenderPassResult->renderPass->getHandle();
        auto framebuffer =mSwapchainFramebuffers[imageIndex];
        auto extent = mWindowContext->getSwapchainExtent();

        passBeginInfo.reset()
            .addClearColor({0.12f, 0.12f, 0.12f, 1.0f})
            .addClearDepth({ 1.0f, 0 })
            .update(renderpass,framebuffer,extent);

        context.beginRenderPass(&passBeginInfo.getRenderPassBeginInfo(), VK_SUBPASS_CONTENTS_INLINE);

        auto viewport= std::dynamic_pointer_cast<ViewportComponent>(
            mComponentRegistry->getComponent(PipelineComponentType::VIEWPORT_STATE,"Fullscreen")
        );
        context.setViewport(viewport->getViewports()[0]);
        context.setScissor(viewport->getScissors()[0]);

        context.bindGraphicsPipeline(mGraphicsPipeline);

        // auto VBO = mMesh.getVertexBuffer()->getBufferHandles();
        auto VBO =loader->getVertexBuffers()->getBufferHandles();
        context.bindVertexBuffers(VBO);

        //auto IBO= mMesh.getIndexBuffer()->getBuffer()；
        auto IBO =loader->getIndexBuffers()->getBuffer();
        context.bindIndexBuffer(IBO);

        uint32_t frameIndex = mVulkanBackend.getCurrentFrameIndex();
        VkDescriptorSet descriptorSet = mDescriptorManager->getDescriptorSet(0, frameIndex);
        context.bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, descriptorSet, 0, mPipelineLayout->getHandle());

        //auto index=mMesh.getIndexBuffer()->getIndexCount();
        auto index =loader->getIndexCount();
        context.drawIndexed(index, 1, 0, 0, 0);

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

            try {
                drawFrame();
            }
            catch (...) {
                recreateSwapchain();
                continue;
            }

            if (mFramebufferResized) {
                mFramebufferResized = false;
                recreateSwapchain();
            }
        }

        vkDeviceWaitIdle(mVulkanCore->getLogicalDeviceHandle());
    }

    void Application::cleanupSwapchain() {
        auto device = mVulkanCore->getLogicalDeviceHandle();

        for (auto framebuffer : mSwapchainFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
        mSwapchainFramebuffers.clear();
    }

    void Application::recreateSwapchain() {
        vkDeviceWaitIdle(mVulkanCore->getLogicalDeviceHandle());

        cleanupSwapchain();

        mWindowContext->recreateSwapchain();

        createDepthTexture();

        auto viewportComponent = std::dynamic_pointer_cast<ViewportComponent>(
            mComponentRegistry->getComponent(PipelineComponentType::VIEWPORT_STATE, "Fullscreen"));

        if (viewportComponent) {
            viewportComponent->reset();

            ViewportScissor viewport(mWindowContext->getSwapchainExtent(),true);
            viewportComponent->setViewportScissor(viewport);
        }
        createFramebuffers();
    }

    void Application::cleanup() {
        cleanupSwapchain();

        if (mGraphicsPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(mVulkanCore->getLogicalDeviceHandle(), mGraphicsPipeline, nullptr);
            mGraphicsPipeline = VK_NULL_HANDLE;
        }
        if (mPipelineLayout) {
            mPipelineLayout.reset();
        }

        mMatrixUniformBuffers.clear();
        mColorUniformBuffers.clear();

        if (mDepthTexture) {
            mDepthTexture->cleanup();
        }
        if (mDescriptorManager) {
            mDescriptorManager->cleanup();
        }
        if (mComponentRegistry) {
            mComponentRegistry->clear();
        }
        mPipelineBuilder.reset();
        mRenderPassResult.reset();
        mVulkanBackend.shutdown();
        mWindowContext.reset();
        mVulkanCore.reset();
        mWindow.reset();
    }

    Application::~Application() {
        cleanup();
    }

} // namespace StarryEngine