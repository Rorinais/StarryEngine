#include "Application.hpp"
#include "../renderer/subpassRecorder/GbufferRecorder.hpp"

namespace StarryEngine {
    Application::Application() {
        Window::Config config;
        config.width = m_width;
        config.height = m_height;
        config.title = m_title;
        config.iconPath = m_icon_path;
        config.highDPI = false;
        config.resizable = true;
        config.fullScreen = false;
        m_window = Window::create(config);

        m_rhi = VulkanRHIFactory::createDefault(RHI::API::Vulkan, m_window, m_width, m_height, m_flightFrame);
        if (!m_rhi) LOG_ERROR("Failed to create RHI!");

        createDescriptorPool();
        createRenderer();
        initEventDispatcher();
    }

    void Application::createDescriptorPool() {
        m_resMgr = m_rhi->getResourceManager();

        RHI::DescriptorPoolDesc poolDesc;
        poolDesc.maxSets = 20;
        poolDesc.poolSizes = {
            { RHI::DescriptorType::UniformBuffer, 10 },
            { RHI::DescriptorType::CombinedImageSampler, 10 },
            { RHI::DescriptorType::InputAttachment, 10 }
        };
        poolDesc.freeDescriptorSet = true;
        poolDesc.debugName = "GlobalDescriptorPool";
        m_descriptorPool = m_resMgr->createDescriptorPool(poolDesc);
    }

    bool Application::createGridResources() {
        // ---------- 1. 生成网格顶点和索引数据（与之前相同）----------
        struct GridVertex {
            glm::vec3 position;
            glm::vec3 color;
        };
        std::vector<GridVertex> vertices;
        std::vector<uint32_t> indices;

        const float size = 50.0f;
        const int divisions = 50;
        const float step = size / divisions;
        const float half = size * 0.5f;
        const glm::vec3 colorXAxis(1.0f, 0.0f, 0.0f);
        const glm::vec3 colorYAxis(0.0f, 1.0f, 0.0f);
        const glm::vec3 colorZAxis(0.0f, 0.0f, 1.0f);
        const glm::vec3 colorLine(0.4f, 0.4f, 0.4f);

        // X 方向线条
        for (int i = 0; i <= divisions; ++i) {
            float z = -half + i * step;
            bool isXAxis = (std::abs(z) < 0.001f);
            glm::vec3 col = isXAxis ? colorXAxis : colorLine;
            vertices.push_back({ {-half, 0.0f, z}, col });
            vertices.push_back({ { half, 0.0f, z}, col });
        }

        // Z 方向线条
        for (int i = 0; i <= divisions; ++i) {
            float x = -half + i * step;
            bool isZAxis = (std::abs(x) < 0.001f);
            glm::vec3 col = isZAxis ? colorZAxis : colorLine;
            vertices.push_back({ { x, 0.0f, -half}, col });
            vertices.push_back({ { x, 0.0f,  half}, col });
        }

        // Y 轴线
        vertices.push_back({ {0.0f, -half, 0.0f}, colorYAxis });
        vertices.push_back({ {0.0f,  half, 0.0f}, colorYAxis });

        // 生成索引：每两个连续顶点构成一条线段
        for (uint32_t i = 0; i < vertices.size(); i += 2) {
            indices.push_back(i);
            indices.push_back(i + 1);
        }

        // 将顶点转换为 float 数组（用于 setVertices）
        std::vector<float> vertexData;
        vertexData.reserve(vertices.size() * 6);
        for (const auto& v : vertices) {
            vertexData.push_back(v.position.x);
            vertexData.push_back(v.position.y);
            vertexData.push_back(v.position.z);
            vertexData.push_back(v.color.r);
            vertexData.push_back(v.color.g);
            vertexData.push_back(v.color.b);
        }

        // ---------- 2. 创建网格 Geometry ----------
        m_gridGeometry = std::make_shared<Assets::Geometry>(m_resMgr);
        m_gridGeometry->setVertices(vertexData);
        m_gridGeometry->setIndices(indices);
        m_gridGeometry->setPrimitiveTopology(RHI::PrimitiveTopology::LineList);
        // 设置顶点布局（位置 + 颜色）
        Assets::VertexLayout gridLayout;
        gridLayout.addAttribute(0, 0, RHI::Format::RGB32_Float, 0);                // 位置
        gridLayout.addAttribute(1, 0, RHI::Format::RGB32_Float, 3 * sizeof(float)); // 颜色
        gridLayout.addBinding(0, 6 * sizeof(float), RHI::VertexInputRate::PerVertex); // stride = 6个float
        m_gridGeometry->setVertexLayout(gridLayout);

        // 设置单个子网格
        Assets::Submesh submesh;
        submesh.indexOffset = 0;
        submesh.indexCount = static_cast<uint32_t>(indices.size());
        submesh.materialIndex = 0;
        m_gridGeometry->setSubmeshes({ submesh });

        // 上传到 GPU（新接口）
        if (!m_gridGeometry->uploadToGPU()) {
            LOG_ERROR("Failed to upload grid geometry to GPU");
            return false;
        }
        return true;
    }

    void Application::createRenderer() {
        m_scene = std::make_shared<Scene::Scene>();

        auto geometry = std::make_shared<Assets::Geometry>(m_resMgr);
        std::vector<Assets::MaterialParams> params;
        if (!Assets::ModelLoader::loadFromFile(m_resMgr, "assets/models/Griseo.obj", *geometry, params)) {
            LOG_ERROR("Failed to load model");
            return;
        }
        geometry->uploadToGPU();

        m_renderer = std::make_unique<Renderer>(m_rhi, m_descriptorPool, m_scene);
        m_renderer->createGlobalSetLayout();
        m_renderer->createGlobalUniformBuffer();

        // 推送常量范围（模型矩阵）
        std::vector<RHI::PushConstantRange> pushConstants = {
            {RHI::ShaderStage::Vertex, 0, sizeof(glm::mat4)}
        };

        std::vector<std::shared_ptr<Assets::MaterialInstance>> materialInstances;

        for (auto& param : params) {
            std::unordered_map<uint32_t, RHI::DescriptorSetLayoutHandle> layoutMap;
            layoutMap[0] = m_renderer->getGlobalSetLayout(); // 添加全局 set0 布局

            // 1. 定义描述符集布局（set=1，包含 UBO binding 0 和纹理 binding 1）
            RHI::DescriptorSetLayoutDesc layoutDesc;
            layoutDesc.bindings = {
                {0, RHI::DescriptorType::UniformBuffer, 1, RHI::ShaderStage::Vertex | RHI::ShaderStage::Fragment},
                {1, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Fragment}
            };
            layoutMap[1] = Assets::DescriptorSetLayoutCache::getOrCreateLayout(m_resMgr.get(), layoutDesc); // 材质私有 set1 布局

            std::string fsPath;
            if (param.name == "body") fsPath = "assets/shaders/core/shader.frag";
            else if (param.name == "brow") fsPath = "assets/shaders/core/shader.frag";
            else if (param.name == "eyes") fsPath = "assets/shaders/core/shader.frag";
            else if (param.name == "face") fsPath = "assets/shaders/core/face.frag";
            else fsPath = "assets/shaders/core/hair.frag";

            // 3. 创建材质模板
            auto tmpl = std::make_shared<Assets::DefaultMaterialTemplate>(m_resMgr, layoutMap, pushConstants);
            if (!tmpl->loadShaders("assets/shaders/core/shader.vert", fsPath)) {
                LOG_ERROR("Failed to load shaders for material: {}", param.name);
                continue;
            }

            // 4. 创建材质实例（传入全局 set0）
            auto instance = std::make_shared<Assets::MaterialInstance>(tmpl, m_descriptorPool, m_resMgr.get(), m_renderer->getGlobalDescriptorSet());

            // 5. 设置 UBO 初始数据
            Assets::MaterialUniforms uniforms{};
            instance->setUniform(1, 0, &uniforms, sizeof(Assets::MaterialUniforms));

            // 6. 设置纹理（如果存在）
            if (!param.albedoTexture.empty()) {
                Assets::TextureLoader loader(m_resMgr);
                auto texResult = loader.loadTexture2D(param.albedoTexture, RHI::Format::RGBA8_UNorm, "Albedo");
                if (texResult.texture.isValid()) {
                    instance->setTexture(1, 1, texResult.texture, texResult.sampler);
                }
                else {
                    LOG_ERROR("Failed to load texture: {}", param.albedoTexture);
                }
            }
            else {
                LOG_WARN("Material {} has no albedo texture", param.name);
            }

            instance->setRenderStage(Scene::RenderStage::Forward);
            instance->setRenderQueue(Scene::RenderQueue::Opaque);

            materialInstances.push_back(instance);
        }

        if (!createGridResources()) {
            LOG_ERROR("Failed to create grid geometry");
        }
        else {
            // 创建网格材质模板（布局包含 set0，使用全局 set0 布局）
            std::unordered_map<uint32_t, RHI::DescriptorSetLayoutHandle> gridLayoutMap;
            gridLayoutMap[0] = m_renderer->getGlobalSetLayout(); // 重要：使用全局 set0 布局

            std::vector<RHI::PushConstantRange> gridPushConstants = {
                {RHI::ShaderStage::Vertex, 0, sizeof(glm::mat4)}
            };

            auto gridTmpl = std::make_shared<Assets::DefaultMaterialTemplate>(
                m_resMgr, gridLayoutMap, gridPushConstants);
            if (!gridTmpl->loadShaders("assets/shaders/core/gridShader.vert", "assets/shaders/core/gridShader.frag")) {
                LOG_ERROR("Failed to load grid shaders");
            }
            else {
                // 创建网格材质实例，传入全局 set0 描述符集
                auto gridMaterialInst = std::make_shared<Assets::MaterialInstance>(gridTmpl, m_descriptorPool, m_resMgr.get(), m_renderer->getGlobalDescriptorSet());
                gridMaterialInst->setRenderStage(Scene::RenderStage::Forward);
                gridMaterialInst->setRenderQueue(Scene::RenderQueue::Opaque);

                // 创建网格 RenderObject
                auto gridObj = std::make_shared<Scene::RenderObject>();
                gridObj->geometry = m_gridGeometry;
                gridObj->materials = { gridMaterialInst };
                gridObj->transform = glm::mat4(1.0f);
                m_scene->addObject(gridObj);
            }
        }

        // 7. 创建渲染对象
        auto obj = std::make_shared<Scene::RenderObject>();
        obj->geometry = geometry;
        obj->materials = materialInstances; // 注意：RenderObject::materials 类型需为 vector<MaterialInstancePtr>
        obj->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 2.5f));
        m_rotatingObject = obj;
        m_scene->addObject(obj);

        // 8. 相机设置
        auto perspectiveCamera = std::make_shared<Scene::PerspectiveCamera>();
        perspectiveCamera->setPerspective(glm::radians(45.0f), (float)m_width / m_height, 0.1f, 100.0f);
        perspectiveCamera->lookAt(glm::vec3(0.0f, 2.0f, 5.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        m_scene->addCamera(perspectiveCamera);

        auto orthographicCamera = std::make_shared<Scene::OrthographicCamera>();
        orthographicCamera->setOrthographic(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f);
        orthographicCamera->lookAt(glm::vec3(0.0f, 2.0f, 5.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        m_scene->addCamera(orthographicCamera);

        m_scene->setActiveCamera(perspectiveCamera);

        // 9. 创建渲染路径
        auto renderPath = std::make_unique<ForwardRenderPath>(m_rhi, m_width, m_height);
        auto graph = renderPath->getRenderGraph();

        std::unordered_map<std::string, RHI::TextureDesc> textureDescs;
        RHI::TextureDesc colorDesc;
        colorDesc.extent = { m_width, m_height, 1 };
        colorDesc.format = RHI::Format::RGBA8_UNorm;
        colorDesc.type = RHI::TextureType::Texture2D;
        colorDesc.allowRenderTarget = true;
        textureDescs["Color"] = colorDesc;

        // 添加深度纹理描述
        RHI::TextureDesc depthDesc = colorDesc;
        depthDesc.format = m_rhi->getDepthFormat();
        depthDesc.allowDepthStencil = true;
        depthDesc.allowRenderTarget = false;
        textureDescs["Depth"] = depthDesc;

        RHI::TextureDesc swapchainDesc = colorDesc;
        swapchainDesc.format = RHI::Format::BGRA8_sRGB;
        textureDescs["Swapchain"] = swapchainDesc;

        renderPath->setTextureDescs(textureDescs);

        RenderPathConfig config;
        SubpassConfig opaqueSubpass;
        opaqueSubpass.name = "Opaque";

        SubpassAttachment colorAttach;
        colorAttach.textureName = "Swapchain";
        colorAttach.params.clearColor = RHI::Color{ 0.05f, 0.05f, 0.05f, 1.0f };
        colorAttach.params.loadOp = RHI::AttachmentLoadOp::Clear;
        colorAttach.params.storeOp = RHI::AttachmentStoreOp::Store;
        colorAttach.params.initialLayout = RHI::ImageLayout::Undefined;
        colorAttach.params.finalLayout = RHI::ImageLayout::PresentSrc;
        opaqueSubpass.colorAttachments.push_back(colorAttach);

        SubpassAttachment depthAttach;
        depthAttach.textureName = "Depth";
        depthAttach.params.clearDepth = 1.0f;
        depthAttach.params.loadOp = RHI::AttachmentLoadOp::Clear;
        depthAttach.params.storeOp = RHI::AttachmentStoreOp::DontCare;
        depthAttach.params.initialLayout = RHI::ImageLayout::Undefined;
        depthAttach.params.finalLayout = RHI::ImageLayout::DepthStencilAttachment;
        opaqueSubpass.depthAttachment = depthAttach;

        opaqueSubpass.recorder = std::make_shared<RenderGraph::MeshDrawRecorder>(m_resMgr);

        config[Scene::RenderStage::Forward][Scene::RenderQueue::Opaque] = opaqueSubpass;

        renderPath->setConfig(config);
        if (!renderPath->initialize()) {
            LOG_ERROR("Failed to initialize renderPath");
            return;
        }
        m_renderer->setRenderPath(std::move(renderPath));
    }

    void Application::run() {
        auto frameContext = m_rhi->getFrameContext();
        FrameMonitor monitor(m_window, frameContext, m_flightFrame);

        while (!glfwWindowShouldClose(m_window->getHandle())) {
            glfwPollEvents();
            monitor.tick();

            if (m_framebufferResized) {
                m_rhi->waitIdle();
                m_framebufferResized = false;
                if (m_width == 0 || m_height == 0) continue;

                if (!m_rhi->recreateSwapChain(m_width, m_height)) {
                    std::cerr << "Failed to recreate swap chain!" << std::endl;
                    continue;
                }
                m_renderer->onResize(m_width, m_height);

                continue;
            }
            if (m_width == 0 || m_height == 0) continue;

            float deltaTime = monitor.getDeltaTime();
            if (m_cameraController) {
                m_cameraController->update(deltaTime);
            }

            bool success = m_rhi->renderFrame([this, deltaTime](RHI::RHICommandEncoder* encoder, uint32_t imageIndex) {
                m_renderer->renderFrame(encoder, imageIndex, deltaTime);
                });
            monitor.updateTitle();
        }
    }

    void Application::initEventDispatcher() {
        GetEventDispatcher().subscribe(EventType::KeyPressed, [this](IEvent& e) {
            auto& ev = static_cast<KeyEvent&>(e);
            if (ev.getKey() == GLFW_KEY_ESCAPE && ev.getAction() == GLFW_PRESS) {
                glfwSetWindowShouldClose(m_window->getHandle(), GLFW_TRUE);
            }
            });

        GetEventDispatcher().subscribe(EventType::WindowResize, [this](IEvent& e) {
            auto& ev = static_cast<WindowResizeEvent&>(e);
            m_width = ev.getWidth();
            m_height = ev.getHeight();
            m_framebufferResized = true;

            if (m_scene && m_scene->getActiveCamera()) {
                auto cam = std::dynamic_pointer_cast<Scene::PerspectiveCamera>(m_scene->getActiveCamera());
                if (cam) {
                    cam->setPerspective(cam->getFov(), (float)m_width / m_height, cam->getNear(), cam->getFar());
                    cam->updateProjection();
                }
            }
            });

        // 订阅相机切换事件
        GetEventDispatcher().subscribe(EventType::CameraSwitch, [this](IEvent& e) {
            auto& ev = static_cast<CameraSwitchEvent&>(e);
            uint32_t index = ev.getCameraIndex();
            if (index < m_scene->getCameras().size()) {
                auto oldCamera = m_scene->getActiveCamera();
                auto newCamera = m_scene->getCamera(index);
                if (oldCamera && newCamera) {
                    glm::mat4 oldView = oldCamera->getViewMatrix();
                    glm::vec3 oldPos = oldCamera->getPosition();
                    m_scene->setActiveCamera(newCamera);
                    if (m_cameraController) {
                        m_cameraController->setCamera(newCamera, oldView, oldPos);
                    }
                }
            }
            });

        // 数字键切换相机
        GetEventDispatcher().subscribe(EventType::KeyPressed, [this](IEvent& e) {
            auto& ev = static_cast<KeyEvent&>(e);
            if (ev.getAction() == GLFW_PRESS) {
                int key = ev.getKey();
                if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
                    uint32_t index = key - GLFW_KEY_1;
                    GetEventDispatcher().dispatch<CameraSwitchEvent>(index);
                }
            }
            });

        if (m_scene->getActiveCamera()) {
            m_cameraController = std::make_unique<CameraController>(m_scene->getActiveCamera());
            m_cameraController->setEnabled(false);

            // 鼠标按钮事件：左键激活控制，右键退出控制
            GetEventDispatcher().subscribe(EventType::MouseButtonPressed, [this](IEvent& e) {
                auto& ev = static_cast<MouseButtonEvent&>(e);
                if (ev.getButton() == GLFW_MOUSE_BUTTON_LEFT && ev.getAction() == GLFW_PRESS && !m_controlActive) {
                    // 进入控制模式
                    m_controlActive = true;
                    glfwSetInputMode(m_window->getHandle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    m_cameraController->setEnabled(true);
                }
                else if (ev.getButton() == GLFW_MOUSE_BUTTON_RIGHT && ev.getAction() == GLFW_PRESS && m_controlActive) {
                    // 退出控制模式
                    m_controlActive = false;
                    glfwSetInputMode(m_window->getHandle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    m_cameraController->setEnabled(false);
                }
                });

            // 键盘事件
            GetEventDispatcher().subscribe(EventType::KeyPressed, [this](IEvent& e) {
                if (!m_controlActive) return;
                auto& ev = static_cast<KeyEvent&>(e);
                m_cameraController->onKeyPressed(ev.getKey(), ev.getAction());
                });

            // 鼠标移动事件
            GetEventDispatcher().subscribe(EventType::MouseMoved, [this](IEvent& e) {
                if (!m_controlActive) return;
                auto& ev = static_cast<MouseMoveEvent&>(e);
                m_cameraController->onMouseMoved(ev.getX(), ev.getY());
                });

            // 鼠标滚轮事件
            GetEventDispatcher().subscribe(EventType::MouseScrolled, [this](IEvent& e) {
                auto& ev = static_cast<MouseScrollEvent&>(e);
                if (!m_controlActive) return;  // 仅在控制模式下生效

                // 检查 Ctrl 键是否按下
                bool ctrlPressed = glfwGetKey(m_window->getHandle(), GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                    glfwGetKey(m_window->getHandle(), GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;

                float delta = static_cast<float>(ev.getYOffset()) * 1.0f;
                if (ctrlPressed) {
                    // Ctrl+滚轮：调整视野（FOV）
                    m_cameraController->setFov(delta);
                }
                else {
                    // 普通滚轮：调整移动速度（沿用原有逻辑）
                    m_cameraController->onMouseScrolled(ev.getXOffset(), ev.getYOffset());
                }
                });
        }
    }

    Application::~Application() {
        if (m_rhi) m_rhi->waitIdle();
        m_rhi.reset();
        m_window.reset();
    }
}

int main() {
#ifdef __linux__
    char exePath[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (count != -1) {
        exePath[count] = '\0';
        char* lastSlash = strrchr(exePath, '/');
        if (lastSlash) {
            *lastSlash = '\0';
            std::string layerPath = std::string(exePath) + "/layers";
            setenv("VK_LAYER_PATH", layerPath.c_str(), 1);
            std::string libPath = std::string(exePath);
            std::string currentLdPath = getenv("LD_LIBRARY_PATH") ? getenv("LD_LIBRARY_PATH") : "";
            setenv("LD_LIBRARY_PATH", (libPath + ":" + currentLdPath).c_str(), 1);
        }
    }
#elif _WIN32
    _putenv_s("VK_LAYER_PATH", "layers");
#endif
    StarryEngine::Logger::init();
    StarryEngine::Logger::setShowSourceLoc(true);
    StarryEngine::Application app;
    app.run();
    StarryEngine::Logger::shutdown();
}