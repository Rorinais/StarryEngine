#include "Application.hpp"
#include "../../renderer/core/RenderSystemFactory.hpp"
#include "../../renderer/resources/models/geometry/shape/Cube.hpp"
#include "../../renderer/resources/shaders/ShaderProgram.hpp"
#include "../../renderer/resources/shaders/ShaderBuilder.hpp"
#include "../../renderer/pipeline/pipeline.hpp"
#include "../../renderer/resources/models/mesh/Mesh.hpp"
#include "../../renderer/core/RenderGraph/PipelineBuilder.hpp"
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
        setupPipelineResource();
        setupRenderGraph();
    }

    void Application::setupPipelineResource() {
        // 创建着色器程序
        shaderProgram = ShaderProgram::create(vulkanCore->getLogicalDevice());

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
        shaderProgram->addGLSLStringStage(vertexShader, VK_SHADER_STAGE_VERTEX_BIT, "main", {}, "VertexShader");

        // 片段着色器
        ShaderBuilder fragBuilder(ShaderType::Fragment, "#version 450\n#extension GL_KHR_vulkan_glsl : enable");
        fragBuilder.addInput("vec3", "fragColor", 0);
        fragBuilder.addOutput("vec4", "outColor", 0);
        fragBuilder.setMainBody(R"(
            outColor = vec4(fragColor, 1.0);
        )");
        std::string fragmentShader = fragBuilder.getSource();
        shaderProgram->addGLSLStringStage(fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT, "main", {}, "FragmentShader");

        auto mCube = Cube::create();
        auto geometry = mCube->generateGeometry();
        mesh = Mesh(geometry, vulkanCore->getLogicalDevice(), windowContext->getCommandPool());
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
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        if (descriptorSet!=VK_NULL_HANDLE) {
            context.bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, descriptorSet, 0);
        }

        context.bindVertexBuffers(mesh.getVertexBuffer()->getBufferHandles());
     
        context.bindIndexBuffer(mesh.getIndexBuffer()->getBuffer());

        context.drawIndexed(mesh.getVertexBuffer()->getVertexCount(), 1, 0, 0, 0);
    }

    Pipeline::Ptr Application::createGeometryPipeline(PipelineBuilder& builder) {
        VertexInput vertexInput;
        for (const auto& binding : mesh.getVertexBuffer()->getBindingDescriptions()) {
            vertexInput.addBinding(binding.binding, binding.stride);
        }
        for (const auto& attr : mesh.getVertexBuffer()->getAttributeDescriptions()) {
            vertexInput.addAttribute(attr.binding, attr);
        }

        auto raterization = Rasterization()
            .setCullMode(VK_CULL_MODE_BACK_BIT)
            .setFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .setPolygonMode(VK_POLYGON_MODE_FILL)
            .setLineWidth(1.0f);

        auto depthStencil = DepthStencil()
            .enableDepthTest(VK_TRUE)
            .enableDepthWrite(VK_TRUE)
			.setDepthCompareOp(VK_COMPARE_OP_LESS);
        
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

		auto inputAssembly = InputAssembly()
            .setTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .enablePrimitiveRestart(VK_FALSE);

		auto multiSample = MultiSample()
            .enableSampleShading(VK_FALSE)
            .setSampleCount(VK_SAMPLE_COUNT_1_BIT);

		auto dynamic = Dynamic().setEnableDynamic(true);

        auto viewport= Viewport()
            .IsOpenglCoordinate()
            .init(windowContext->getSwapchainExtent());

        return builder.setColorBlend(colorBlend)
            .setDepthStencil(depthStencil)
            .setInputAssembly(inputAssembly)
            .setMultisample(multiSample)
            .setRasterization(raterization)
            .setDynamic(dynamic)
            .setViewport(viewport)
            .setVertexInput(vertexInput)
            .setShaderProgram(shaderProgram)
            .setDescriptorSetLayouts(descriptorSetLayouts)
            .build();
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