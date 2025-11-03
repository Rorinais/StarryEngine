#include "Application.hpp"
#include "../../renderer/core/RenderSystemFactory.hpp"
#include "../../renderer/resources/models/geometry/shape/Cube.hpp"
#include "../../renderer/resources/shaders/ShaderBuilder.hpp"
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

        // 使用工厂创建渲染器
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

    }

    void Application::setupResources() {
        // 创建几何体（立方体）- 使用静态创建方法
        mCube = Cube::create(1.0f, 1.0f, 1.0f);

        // 创建顶点和索引缓冲区
        createVertexBuffer();
        createIndexBuffer();

        // 创建描述符集和管线布局
        createDescriptorSets();

        // 创建着色器程序
        auto* backend = renderer->getBackend();
        auto device = backend->getDevice();

        ShaderProgram::Ptr shaderProgram = ShaderProgram::create(vulkanCore->getLogicalDevice());

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

        createGraphicsPipeline(shaderProgram);
    }

    void Application::createVertexBuffer() {
        if (!mCube) return;

        // 生成几何体数据
        auto geometry = mCube->generateGeometry();
        if (!geometry) return;

        // 获取顶点数据 - 这里需要根据你的 Geometry 类实现来获取数据
        // 假设 Geometry 类有获取顶点数据的方法
        auto vertices = geometry->getVertices(); // 你需要实现这个方法
        size_t vertexDataSize = vertices.size() * sizeof(Vertex);

        // 通过资源管理器创建顶点缓冲区
        auto* resourceManager = renderer->getResourceManager();
        BufferDesc vertexBufferDesc{
            .size = vertexDataSize,
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU
        };

        // 保存 ResourceHandle，而不是 VkBuffer
        mVertexBufferHandle = resourceManager->createBuffer("VertexBuffer", vertexBufferDesc);

        // 上传顶点数据到缓冲区
        if (mVertexBufferHandle.isValid()) {
            VkBuffer buffer = resourceManager->getBuffer(mVertexBufferHandle);
            void* mappedData = resourceManager->getBufferMappedPointer(mVertexBufferHandle);
            if (mappedData && !vertices.empty()) {
                memcpy(mappedData, vertices.data(), vertexDataSize);
            }
            // 如果没有映射指针，你可能需要使用暂存缓冲区来上传数据
        }
    }

    void Application::createIndexBuffer() {
        if (!mCube) return;

        // 生成几何体数据
        auto geometry = mCube->generateGeometry();
        if (!geometry) return;

        // 获取索引数据 - 这里需要根据你的 Geometry 类实现来获取数据
        auto indices = geometry->getIndices(); // 你需要实现这个方法
        size_t indexDataSize = indices.size() * sizeof(uint32_t);

        // 通过资源管理器创建索引缓冲区
        auto* resourceManager = renderer->getResourceManager();
        BufferDesc indexBufferDesc{
            .size = indexDataSize,
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU
        };

        // 保存 ResourceHandle，而不是 VkBuffer
        mIndexBufferHandle = resourceManager->createBuffer("IndexBuffer", indexBufferDesc);

        // 上传索引数据到缓冲区
        if (mIndexBufferHandle.isValid()) {
            VkBuffer buffer = resourceManager->getBuffer(mIndexBufferHandle);
            void* mappedData = resourceManager->getBufferMappedPointer(mIndexBufferHandle);
            if (mappedData && !indices.empty()) {
                memcpy(mappedData, indices.data(), indexDataSize);
            }
            // 如果没有映射指针，你可能需要使用暂存缓冲区来上传数据
        }
    }

    void Application::createDescriptorSets() {
        // 简化的描述符集创建
        // 实际实现需要根据你的描述符池和布局创建
        mDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        // TODO: 创建描述符集
    }

    void Application::createGraphicsPipeline(ShaderProgram::Ptr shaderProgram) {
        // 简化的图形管线创建
        // 实际实现需要根据你的管线创建API
        // TODO: 创建渲染通道、管线布局和图形管线
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
        // 需要通过资源管理器来更新缓冲区数据
        // 这需要根据你的具体实现来完善
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