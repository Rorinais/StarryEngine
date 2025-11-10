#include "Application.hpp"
#include "../../renderer/core/RenderSystemFactory.hpp"
#include "../../renderer/resources/models/geometry/shape/Cube.hpp"
#include "../../renderer/resources/shaders/ShaderBuilder.hpp"
#include "../../renderer/pipeline/pipeline.hpp"
#include "../../renderer/resources/models/mesh/Mesh.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

namespace StarryEngine {
    struct UniformBuffers {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };

    Application::Application() {
        Window::Config config{
            .width = mWidth,
            .height = mHeight,
            .title = "StarryEngine",
            .resizable = true,
            .monitorIndex = 0,
            .fullScreen = false,
            .highDPI = false,
            .iconPath = "assets/icons/window_icon.png"
        };
        window = Window::create(config);

        window->setKeyCallback([this](int key, int action) {
            if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
                glfwSetWindowShouldClose(window->getHandle(), GLFW_TRUE);
            }
            });

        window->setResizeCallback([this](int width, int height) {
            mFramebufferResized = true;
            mWidth = width;
            mHeight = height;
            });

        vulkanCore = VulkanCore::create();
        vulkanCore->init(window);

        auto commandPool = CommandPool::create(vulkanCore->getLogicalDevice());

        windowContext = WindowContext::create();
        windowContext->init(vulkanCore, window, commandPool);

        renderer = RenderSystemFactory::createDefaultRenderer(vulkanCore, windowContext);
        if (!renderer) {
            throw std::runtime_error("Failed to create renderer!");
        }

        setupResources();
        setupRenderGraph();

        mVertexBufferHandle = ResourceHandle();
        mIndexBufferHandle = ResourceHandle();
    }

    void Application::setupRenderGraph() {
        auto renderGraph = renderer->getRenderGraph();

        auto colorAttachment = renderGraph->createResource("ColorAttachment",
            createAttachmentDescription(
                windowContext->getSwapchainFormat(),
                { window->getWidth(), window->getHeight(), 1 },
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
            )
        );

        auto depthAttachment = renderGraph->createResource("DepthAttachment",
            createAttachmentDescription(
                VK_FORMAT_D32_SFLOAT,
                { window->getWidth(), window->getHeight(), 1 },
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
            )
        );

        auto cameraUBO = renderGraph->createResource("CameraUBO",
            createBufferDescription(
                sizeof(UniformBuffers),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
            )
        );

        renderGraph->addPass("GeometryPass", [&](RenderPass& pass) {
            pass.declareWrite(colorAttachment, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
            pass.declareWrite(depthAttachment, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
            pass.declareRead(cameraUBO, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);

            // renderPipeline创建回调
            pass.setPipelineCreation(
                std::bind(&Application::createGeometryPipeline, this, std::placeholders::_1)
            );
            //renderPass逻辑回调
            pass.setExecutionLogic(
                std::bind(&Application::executeGeometryPass, this, std::placeholders::_1, std::placeholders::_2)
            );
        });
    }

    void Application::executeGeometryPass(CommandBuffer* cmdBuffer, RenderContext& context) {
        if (!mCube) return;

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = mGeometryRenderPass;
        renderPassInfo.framebuffer = mGeometryFramebuffers[context.getFrameIndex()];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = { mWidth, mHeight };

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = { {0.1f, 0.2f, 0.3f, 1.0f} };
        clearValues[1].depthStencil = { 1.0f, 0 };
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        context.beginRenderPass(&renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        context.bindGraphicsPipeline("GeometryPipeline");

        VkDescriptorSet descriptorSet = mDescriptorSets[context.getFrameIndex()];
        context.bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, descriptorSet, 0);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(mWidth);
        viewport.height = static_cast<float>(mHeight);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        context.setViewport(viewport);

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = { mWidth, mHeight };
        context.setScissor(scissor);

        if (mVertexBufferHandle.isValid()) {
            context.bindVertexBuffer(mVertexBufferHandle);
        }

        if (mIndexBufferHandle.isValid()) {
            context.bindIndexBuffer(mIndexBufferHandle);
        }

        context.drawIndexed(mCube->getIndexCount(), 1, 0, 0, 0);

        context.endRenderPass();
    }

    Pipeline::Ptr Application::createGeometryPipeline(PipelineBuilder& builder) {
        // 配置管线状态
        builder.setColorBlend(getColorBlendState())
            .setDepthStencil(getDepthStencilState())
            .setInputAssembly(getInputAssemblyState())
            .setMultisample(getMultisampleState())
            .setRasterization(getRasterizationState())
            .setDynamic(getDynamicState())
            .setViewport(getViewportState())
            .setVertexInput(getCubeVertexInput())
            .setShaderProgram(mShaderProgram);

        // 设置描述符集布局
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts = getDescriptorSetLayouts();
        builder.setDescriptorSetLayouts(descriptorSetLayouts);

        return builder.build();
    }

    VertexInput Application::getCubeVertexInput() const {
        if (!mCube) return VertexInput();

        auto geometry = mCube->generateGeometry();

        auto mesh = Mesh(geometry,vulkanCore->getLogicalDevice(),windowContext->getCommandPool());
        auto bindings = mesh.getVertexBuffer()->getBindingDescriptions();
        auto attributes = mesh.getVertexBuffer()->getAttributeDescriptions();

        VertexInput vertexInput;
        for (const auto& binding : bindings) {
            vertexInput.addBinding(binding.binding, binding.stride);
        }
        for (const auto& attr : attributes) {
            vertexInput.addAttribute(attr.binding, attr);
        }

        return vertexInput;
    }

    Rasterization Application::getRasterizationState() const {
        return Rasterization()
            .setCullMode(VK_CULL_MODE_BACK_BIT)
            .setFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .setPolygonMode(VK_POLYGON_MODE_FILL)
            .setLineWidth(1.0f);
    }

    DepthStencil Application::getDepthStencilState() const {
        return DepthStencil()
            .enableDepthTest(VK_TRUE)
            .enableDepthWrite(VK_TRUE)
            .setDepthCompareOp(VK_COMPARE_OP_LESS);
    }

    ColorBlend Application::getColorBlendState() const {
        return ColorBlend().addAttachment(
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
    }

    InputAssembly Application::getInputAssemblyState() const {
        return InputAssembly()
            .setTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .enablePrimitiveRestart(VK_FALSE);
    }

    MultiSample Application::getMultisampleState() const {
        return MultiSample()
            .enableSampleShading(VK_FALSE)
            .setSampleCount(VK_SAMPLE_COUNT_1_BIT);
    }

    Dynamic Application::getDynamicState() const {
        return Dynamic().setEnableDynamic(true);
    }

    Viewport Application::getViewportState() const {
        return Viewport()
            .IsOpenglCoordinate()
            .init(windowContext->getSwapchainExtent());
    }

    std::vector<VkDescriptorSetLayout> Application::getDescriptorSetLayouts() const {
        std::vector<VkDescriptorSetLayout> layouts;
        if (mDescriptorSetLayout != VK_NULL_HANDLE) {
            layouts.push_back(mDescriptorSetLayout);
        }
        return layouts;
    }

    void Application::setupResources() {
        // 创建着色器程序
        mShaderProgram = ShaderProgram::create(vulkanCore->getLogicalDevice());

        // 顶点着色器
        ShaderBuilder vertBuilder(ShaderType::Vertex, "#version 450\n#extension GL_KHR_vulkan_glsl : enable");
        vertBuilder.addInput("vec3", "inPosition", 0);
        vertBuilder.addOutput("vec3", "fragColor", 0);
        vertBuilder.addUniformBuffer("CameraUBO", 0, 0, { "mat4 viewProj" });
        vertBuilder.setMainBody(R"(
            gl_Position = ubo.viewProj * vec4(inPosition, 1.0);
            fragColor = vec3(1.0, 1.0, 1.0);
        )");
        std::string vertexShader = vertBuilder.getSource();
        mShaderProgram->addGLSLStringStage(vertexShader, VK_SHADER_STAGE_VERTEX_BIT, "main", {}, "VertexShader");

        // 片段着色器
        ShaderBuilder fragBuilder(ShaderType::Fragment, "#version 450\n#extension GL_KHR_vulkan_glsl : enable");
        fragBuilder.addInput("vec3", "fragColor", 0);
        fragBuilder.addOutput("vec4", "outColor", 0);
        fragBuilder.setMainBody(R"(
            outColor = vec4(fragColor, 1.0);
        )");
        std::string fragmentShader = fragBuilder.getSource();
        mShaderProgram->addGLSLStringStage(fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT, "main", {}, "FragmentShader");
    }

    void Application::run() {
        while (!glfwWindowShouldClose(window->getHandle())) {
            glfwPollEvents();

            if (mFramebufferResized) {
                mFramebufferResized = false;
                renderer->onSwapchainRecreated();
            }

            static auto startTime = std::chrono::high_resolution_clock::now();
            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float>(currentTime - startTime).count();

            // 更新Uniform Buffer数据
            updateUniformBuffer(time);

            // 渲染帧
            renderer->render();
        }

        // 等待设备空闲再退出
        vkDeviceWaitIdle(vulkanCore->getLogicalDeviceHandle());
    }

    void Application::updateUniformBuffer(float time) {
        UniformBuffers ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(1.0f, 1.0f, 1.0f));
        ubo.view = glm::lookAt(
            glm::vec3(2.0f, 2.0f, 2.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f));
        ubo.proj = glm::perspective(
            glm::radians(30.0f),
            window->getAspectRatio(),
            0.1f,
            100.0f);

        glm::mat4 viewProj = ubo.proj * ubo.view * ubo.model;

        // TODO: 更新Uniform Buffer
    }

    Application::~Application() {
        // 按正确顺序销毁资源
        renderer.reset();
        windowContext.reset();

        if (vulkanCore && vulkanCore->getLogicalDeviceHandle()) {
            vkDeviceWaitIdle(vulkanCore->getLogicalDeviceHandle());
            vulkanCore.reset();
        }

        window.reset();
    }
}