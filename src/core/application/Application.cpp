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

        //auto renderGraph = RenderGraph::create();

        //auto mainPass = VulkanRenderPass::create(RenderPass::MAIN);

        //VulkanRenderPass::AttachmentConfig colorAttachment{
        //    .format = windowContext->getSwapchainFormat(),
        //    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        //    .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        //    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        //    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        //    .isDepth = false
        //};

        //VulkanRenderPass::AttachmentConfig depthAttachment{
        //    .format = Texture::findSupportedDepthFormat(vulkanCore->getPhysicalDeviceHandle()),
        //    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        //    .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        //    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        //    .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        //    .isDepth = true
        //};

        //VulkanRenderPass::DependencyConfig dependency{
        //    .srcSubpass = VK_SUBPASS_EXTERNAL,
        //    .dstSubpass = 0,
        //    .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        //                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        //    .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        //                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        //    .srcAccessMask = 0,
        //    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        //                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
        //};

        //mainPass->addAttachment(colorAttachment)
        //    .addAttachment(depthAttachment)
        //    .addDependency(dependency);

        ShaderProgram::Ptr shaderProgram = ShaderProgram::create(vulkanCore->getLogicalDevice());

        ShaderBuilder vertBuilder(ShaderType::Vertex, "#version 450\n#extension GL_KHR_vulkan_glsl : enable");
        vertBuilder.addInput("vec3", "inPosition", 0);
        vertBuilder.addOutput("vec3", "fragColor", 0);
        vertBuilder.addUniformBuffer("CameraUBO", 0, 0, { "mat4 viewProj" });
        vertBuilder.setMainBody(R"(
            gl_Position = ubo.viewProj*vec4(inPosition,1.0);
            //gl_Position = vec4(inPosition,1.0);
            fragColor = vec3(1.0,1.0,1.0);
         )");
        std::string vertexShader = vertBuilder.getSource();
        std::cout << "=== Vertex Shader ===\n" << vertexShader << "\n\n";
        shaderProgram->addGLSLStringStage(vertexShader, VK_SHADER_STAGE_VERTEX_BIT, "main", {}, "VertexShader");

        ShaderBuilder fragBuilder(ShaderType::Fragment, "#version 450\n#extension GL_KHR_vulkan_glsl : enable");
        fragBuilder.addInput("vec3", "fragColor", 0);
        fragBuilder.addOutput("vec4", "outColor", 0);
        fragBuilder.setMainBody(R"(
            outColor = vec4(fragColor,1.0);
        )");
        std::string fragmentShader = fragBuilder.getSource();
        std::cout << "=== Fragment Shader ===\n" << fragmentShader << "\n";
        shaderProgram->addGLSLStringStage(fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT, "main", {}, "FragmentShader");

        //Cube cube;
        //Mesh mesh(cube.generateGeometry(), vulkanCore->getLogicalDevice(), commandPool);

        //mainPass->setShaderProgram(shaderProgram);
        //mainPass->setMesh(mesh);

        //renderGraph->addPass(mainPass);
        //renderGraph->setMainPass(mainPass);

        //auto rs = ResourceManager::create(vulkanCore->getLogicalDevice(), windowContext->getCommandPool());
        //rs->registerUniformBuffer("CameraUBO", sizeof(glm::mat4), MAX_FRAMES_IN_FLIGHT);
        //renderGraph->setResourceManager(rs);

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

            /*ubo.proj[1][1] *= -1;*/

            glm::mat4 viewProj = ubo.proj * ubo.view * ubo.model;
            auto resourceManager = renderer->getRenderGraph()->getResourceManager();
            resourceManager->updateUniformBuffer("CameraUBO", renderer->getCurrentFrame(), &viewProj, sizeof(glm::mat4));

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