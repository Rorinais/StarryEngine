#include "Application.hpp"
#include "../../renderer/resources/models/geometry/shape/Cube.hpp"
#include "../../renderer/resources/shaders/ShaderBuilder.hpp"

namespace StarryEngine {
    struct UniformBuffers {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };

    Application::Application() {
        Window::Config config{
            .width = 800,
            .height = 600,
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

        window->setResizeCallback([this](int, int) {
            mFramebufferResized = true;
            });

        vulkanCore = VulkanCore::create();
        vulkanCore->init(window);

        auto commandPool = CommandPool::create(vulkanCore->getLogicalDevice());

        windowContext = WindowContext::create();
        windowContext->init(vulkanCore, window, commandPool);

        renderer = VulkanRenderer::create();
        renderer->init(vulkanCore, windowContext);

        ShaderProgram::Ptr shaderProgram = ShaderProgram::create(vulkanCore->getLogicalDevice());

        ShaderBuilder vertBuilder(ShaderType::Vertex, "#version 450\n#extension GL_KHR_vulkan_glsl : enable");
        vertBuilder.addInput("vec3", "inPosition", 0);
        vertBuilder.addOutput("vec3", "fragColor", 0);
        vertBuilder.addUniformBuffer("CameraUBO", 0, 0, { "mat4 viewProj" });
        vertBuilder.setMainBody(R"(
            gl_Position = ubo.viewProj*vec4(inPosition,1.0);
            fragColor = vec3(1.0,1.0,1.0);
         )");
        std::string vertexShader = vertBuilder.getSource();
        shaderProgram->addGLSLStringStage(vertexShader, VK_SHADER_STAGE_VERTEX_BIT, "main", {}, "VertexShader");

        ShaderBuilder fragBuilder(ShaderType::Fragment, "#version 450\n#extension GL_KHR_vulkan_glsl : enable");
        fragBuilder.addInput("vec3", "fragColor", 0);
        fragBuilder.addOutput("vec4", "outColor", 0);
        fragBuilder.setMainBody(R"(
            outColor = vec4(fragColor,1.0);
        )");
        std::string fragmentShader = fragBuilder.getSource();
        shaderProgram->addGLSLStringStage(fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT, "main", {}, "FragmentShader");

	
        // 创建RenderGraph
        auto renderGraph = std::make_shared<RenderGraph>(
            vulkanCore->getLogicalDeviceHandle(),
            vulkanCore->getAllocator()
        );

        // 创建资源
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
                sizeof(glm::mat4),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
            )
        );

        // 添加几何通道
        renderGraph->addPass("geometryPass", [&](RenderPass& pass) {
            // 声明资源读写关系
            pass.declareWrite(colorAttachment, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
            pass.declareWrite(depthAttachment, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
            pass.declareRead(cameraUBO, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);

            pass.setExecutionLogic([this, colorAttachment, depthAttachment](CommandBuffer* cmdBuffer, RenderContext& context) {
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

				// 开始渲染通道
                context.beginRenderPass(cmdBuffer->getHandle(), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

                // 使用RenderContext绑定管线
                context.bindGraphicsPipeline("GeometryPipeline");

                // 绑定描述符集
                VkDescriptorSet descriptorSet = mDescriptorSets[context.getFrameIndex()];
                context.bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, descriptorSet, 0);

                // 设置动态状态
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

                // 使用RenderContext绑定缓冲区
                context.bindVertexBuffer(mVertexBufferHandle);
                context.bindIndexBuffer(mIndexBufferHandle);

                // 绘制
                context.drawIndexed(mMesh.getIndexCount(), 1, 0, 0, 0);

                // 结束渲染通道
                context.endRenderPass(cmdBuffer->getHandle());
                });
            });

        // 编译RenderGraph
        if (!renderGraph->compile()) {
            throw std::runtime_error("Failed to compile render graph!");
        }

        renderer->setRenderGraph(renderGraph);
    }

    void Application::run() {
        while (!glfwWindowShouldClose(window->getHandle())) {
            glfwPollEvents();
            if (mFramebufferResized) {
                mFramebufferResized = false;
                renderer->recreateSwapChain();
            }

            static auto startTime = std::chrono::high_resolution_clock::now();
            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float>(currentTime - startTime).count();

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

            // 更新Uniform Buffer - 需要根据新的ResourceSystem调整
            /* 示例代码
            auto renderGraph = renderer->getRenderGraph();
            if (renderGraph) {
                auto& resourceRegistry = renderGraph->getResourceRegistry();
                // 更新Uniform Buffer数据
                // resourceRegistry.updateBufferData("CameraUBO", renderer->getCurrentFrame(), &viewProj, sizeof(glm::mat4));
            }
            */

            renderer->beginFrame();
            renderer->renderFrame();
            renderer->endFrame();
        }

        // 等待设备空闲再退出
        vkDeviceWaitIdle(vulkanCore->getLogicalDeviceHandle());
    }

    Application::~Application() {
        renderer.reset();
        windowContext.reset();

        if (vulkanCore && vulkanCore->getLogicalDeviceHandle()) {
            vkDeviceWaitIdle(vulkanCore->getLogicalDeviceHandle());
            vulkanCore.reset();
        }

        window.reset();
    }
}