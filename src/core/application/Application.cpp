#include "Application.hpp"

namespace StarryEngine {
    struct MultiMaterialVertex {
        glm::vec3 position;
        glm::vec3 normal;
        uint32_t materialID;  
    };

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
            .width = mExtent.width,
            .height = mExtent.height,
            .title = "StarryEngine",
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
            mExtent.width = width;
            mExtent.height = height;
        });

        mRenderer = std::make_shared<VulkanRenderer>();
        mRenderer->init(mWindow);
        Buffer::SetVMAAllocator(mRenderer->getBackendAs<VulkanBackend>()->getAllocator());
        mDevice =mRenderer->getBackendAs<VulkanBackend>()->getVulkanCore()->getLogicalDevice();
        mCommandPool =mRenderer->getBackendAs<VulkanBackend>()->getWindowContext()->getCommandPool();
        registerDefaultComponents();

        // 注释掉原来的模型加载，使用多材质立方体
        /*
        auto cube = Cube::create();
        auto geometry = cube->generateGeometry();
        mMesh = Mesh(geometry, mVulkanCore->getLogicalDevice(), mWindowContext->getCommandPool());

        loader = std::make_shared<ModelLoader>(mVulkanCore, mWindowContext->getCommandPool()); 
        loader->loadMesh("./assets/models/Griseo.fbx");
        loader->generateBuffer();
        */

        // 第一步：创建多材质立方体
        createMultiMaterialCube();
        
        // 第二步：创建多材质着色器
        createMultipleShaders();
        
        // 第三步：创建渲染通道和深度纹理
        createRenderPass();
        createDepthTexture();
        
        // 第四步：创建描述符管理器
        createDescriptorManager();
        
        // 第五步：创建基础管线（可选，用于测试）
        createGraphicsPipeline();
        
        // 第六步：创建多材质管线（这是关键！）
        createMultiplePipelines();
        
        // 第七步：创建帧缓冲
        mRenderer->createFramebuffers(mSwapchainFramebuffers,mDepthTexture->getImageView(),mRenderPassResult->renderPass->getHandle());
        
        std::cout << "Application initialized successfully!" << std::endl;
    }

    void Application::createMultiMaterialCube() {
        std::cout << "Creating multi-material cube..." << std::endl;
        
        // 立方体顶点数据，每个面4个顶点，共24个顶点
        // 为每个顶点分配材质ID
        std::vector<MultiMaterialVertex> vertices;
        
        float size = 0.5f;
        
        // 前面 - 材质ID 0
        vertices.push_back({{-size, -size, size}, {0.0f, 0.0f, 1.0f}, 0});
        vertices.push_back({{size, -size, size}, {0.0f, 0.0f, 1.0f}, 0});
        vertices.push_back({{size, size, size}, {0.0f, 0.0f, 1.0f}, 0});
        vertices.push_back({{-size, size, size}, {0.0f, 0.0f, 1.0f}, 0});
        
        // 后面 - 材质ID 1
        vertices.push_back({{-size, -size, -size}, {0.0f, 0.0f, -1.0f}, 1});
        vertices.push_back({{-size, size, -size}, {0.0f, 0.0f, -1.0f}, 1});
        vertices.push_back({{size, size, -size}, {0.0f, 0.0f, -1.0f}, 1});
        vertices.push_back({{size, -size, -size}, {0.0f, 0.0f, -1.0f}, 1});
        
        // 上面 - 材质ID 2
        vertices.push_back({{-size, size, -size}, {0.0f, 1.0f, 0.0f}, 2});
        vertices.push_back({{-size, size, size}, {0.0f, 1.0f, 0.0f}, 2});
        vertices.push_back({{size, size, size}, {0.0f, 1.0f, 0.0f}, 2});
        vertices.push_back({{size, size, -size}, {0.0f, 1.0f, 0.0f}, 2});
        
        // 下面 - 材质ID 3
        vertices.push_back({{-size, -size, -size}, {0.0f, -1.0f, 0.0f}, 3});
        vertices.push_back({{size, -size, -size}, {0.0f, -1.0f, 0.0f}, 3});
        vertices.push_back({{size, -size, size}, {0.0f, -1.0f, 0.0f}, 3});
        vertices.push_back({{-size, -size, size}, {0.0f, -1.0f, 0.0f}, 3});
        
        // 右面 - 材质ID 4
        vertices.push_back({{size, -size, -size}, {1.0f, 0.0f, 0.0f}, 4});
        vertices.push_back({{size, size, -size}, {1.0f, 0.0f, 0.0f}, 4});
        vertices.push_back({{size, size, size}, {1.0f, 0.0f, 0.0f}, 4});
        vertices.push_back({{size, -size, size}, {1.0f, 0.0f, 0.0f}, 4});
        
        // 左面 - 材质ID 5
        vertices.push_back({{-size, -size, -size}, {-1.0f, 0.0f, 0.0f}, 5});
        vertices.push_back({{-size, -size, size}, {-1.0f, 0.0f, 0.0f}, 5});
        vertices.push_back({{-size, size, size}, {-1.0f, 0.0f, 0.0f}, 5});
        vertices.push_back({{-size, size, -size}, {-1.0f, 0.0f, 0.0f}, 5});
        
        // 索引数据：每个面2个三角形
        std::vector<uint32_t> indices;
        for (int face = 0; face < 6; face++) {
            uint32_t baseVertex = face * 4;
            indices.push_back(baseVertex);
            indices.push_back(baseVertex + 1);
            indices.push_back(baseVertex + 2);
            indices.push_back(baseVertex + 2);
            indices.push_back(baseVertex + 3);
            indices.push_back(baseVertex);
        }
        
        std::cout << "Multi-material cube created: " << vertices.size() 
                  << " vertices, " << indices.size() << " indices" << std::endl;
        
        // 使用VertexArrayBuffer类
        mMultiMaterialVAO = VertexArrayBuffer::create(
            mDevice, 
            mCommandPool
        );
        
        // 创建顶点布局
        VertexLayout layout;
        layout.binding = 0;
        layout.stride = sizeof(MultiMaterialVertex);
        layout.addAttribute(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MultiMaterialVertex, position), "position");
        layout.addAttribute(1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MultiMaterialVertex, normal), "normal");
        layout.addAttribute(2, VK_FORMAT_R32_UINT, offsetof(MultiMaterialVertex, materialID), "materialID");
        
        // 上传顶点数据
        mMultiMaterialVAO->upload<MultiMaterialVertex>(0, vertices, layout);
        
        // 创建索引缓冲区
        mMultiMaterialIBO = std::make_shared<IndexBuffer>(
            mDevice, 
            mCommandPool
        );
        mMultiMaterialIBO->loadData(indices);
        
        mMultiMaterialIndexCount = indices.size();
        
        // 为MultiMaterialVertex生成布局（用于后面的管线配置）
        // 这个需要在VertexArrayBuffer.cpp中添加特化
    }

    void Application::createMultipleShaders() {
        std::cout << "Creating multiple shaders for cube faces..." << std::endl;
        
        mMultiMaterialShaders.resize(6);
        
        // 6个不同的片段着色器内容
        std::vector<std::string> fragmentShaders = {
            // 红色面 - 纯色
            R"(
            #version 450
            layout(location = 0) in vec3 fragPosition;
            layout(location = 1) in vec3 fragNormal;
            layout(location = 2) flat in uint fragMaterialID;
            
            layout(location = 0) out vec4 outColor;
            
            void main() {
                // 红色，加上简单的光照
                vec3 lightPos = vec3(2.0, 2.0, 2.0);
                vec3 lightDir = normalize(lightPos - fragPosition);
                float diff = max(dot(fragNormal, lightDir), 0.0);
                vec3 color = vec3(1.0, 0.0, 0.0);
                outColor = vec4(color * (0.2 + 0.8 * diff), 1.0);
            }
            )",
            
            // 蓝色面 - 棋盘格效果
            R"(
            #version 450
            layout(location = 0) in vec3 fragPosition;
            layout(location = 1) in vec3 fragNormal;
            layout(location = 2) flat in uint fragMaterialID;
            
            layout(location = 0) out vec4 outColor;
            
            void main() {
                vec3 lightPos = vec3(2.0, 2.0, 2.0);
                vec3 lightDir = normalize(lightPos - fragPosition);
                float diff = max(dot(fragNormal, lightDir), 0.0);
                
                // 棋盘格效果
                vec3 pos = fragPosition * 5.0;
                float pattern = mod(floor(pos.x) + floor(pos.y) + floor(pos.z), 2.0);
                vec3 color = mix(vec3(0.0, 0.0, 0.5), vec3(0.2, 0.2, 1.0), pattern);
                
                outColor = vec4(color * (0.3 + 0.7 * diff), 1.0);
            }
            )",
            
            // 绿色面 - 点阵效果
            R"(
            #version 450
            layout(location = 0) in vec3 fragPosition;
            layout(location = 1) in vec3 fragNormal;
            layout(location = 2) flat in uint fragMaterialID;
            
            layout(location = 0) out vec4 outColor;
            
            void main() {
                vec3 lightPos = vec3(2.0, 2.0, 2.0);
                vec3 lightDir = normalize(lightPos - fragPosition);
                float diff = max(dot(fragNormal, lightDir), 0.0);
                
                // 点阵效果
                vec2 uv = fragPosition.xy * 10.0;
                float radius = 0.3;
                float dist = distance(fract(uv), vec2(0.5));
                float pattern = step(radius, dist);
                
                vec3 color = vec3(0.0, pattern * 0.8, pattern * 0.3);
                outColor = vec4(color * (0.2 + 0.8 * diff), 1.0);
            }
            )",
            
            // 黄色面 - 线框效果
            R"(
            #version 450
            layout(location = 0) in vec3 fragPosition;
            layout(location = 1) in vec3 fragNormal;
            layout(location = 2) flat in uint fragMaterialID;
            
            layout(location = 0) out vec4 outColor;
            
            void main() {
                vec3 lightPos = vec3(2.0, 2.0, 2.0);
                vec3 lightDir = normalize(lightPos - fragPosition);
                float diff = max(dot(fragNormal, lightDir), 0.0);
                
                // 线框效果
                vec2 uv = fragPosition.xy * 10.0;
                float lineWidth = 0.1;
                vec2 grid = abs(fract(uv - 0.5) - 0.5);
                float pattern = step(lineWidth, min(grid.x, grid.y));
                
                vec3 color = mix(vec3(1.0, 1.0, 0.0), vec3(0.5, 0.5, 0.0), pattern);
                outColor = vec4(color * (0.3 + 0.7 * diff), 1.0);
            }
            )",
            
            // 紫色面 - 渐变效果
            R"(
            #version 450
            layout(location = 0) in vec3 fragPosition;
            layout(location = 1) in vec3 fragNormal;
            layout(location = 2) flat in uint fragMaterialID;
            
            layout(location = 0) out vec4 outColor;
            
            void main() {
                vec3 lightPos = vec3(2.0, 2.0, 2.0);
                vec3 lightDir = normalize(lightPos - fragPosition);
                float diff = max(dot(fragNormal, lightDir), 0.0);
                
                // 渐变效果
                float gradient = (fragPosition.y + 0.5) / 1.0;
                vec3 color = mix(vec3(0.5, 0.0, 0.5), vec3(1.0, 0.5, 1.0), gradient);
                
                outColor = vec4(color * (0.2 + 0.8 * diff), 1.0);
            }
            )",
            
            // 青色面 - 高光效果
            R"(
            #version 450
            layout(location = 0) in vec3 fragPosition;
            layout(location = 1) in vec3 fragNormal;
            layout(location = 2) flat in uint fragMaterialID;
            
            layout(location = 0) out vec4 outColor;
            
            void main() {
                vec3 lightPos = vec3(2.0, 2.0, 2.0);
                vec3 lightDir = normalize(lightPos - fragPosition);
                vec3 viewDir = normalize(-fragPosition);
                vec3 reflectDir = reflect(-lightDir, fragNormal);
                
                float diff = max(dot(fragNormal, lightDir), 0.0);
                float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
                
                vec3 color = vec3(0.0, 0.8, 0.8);
                vec3 ambient = color * 0.1;
                vec3 diffuse = color * diff;
                vec3 specular = vec3(1.0) * spec;
                
                outColor = vec4(ambient + diffuse + specular, 1.0);
            }
            )"
        };
        
        // 创建6个ShaderProgram
        for (int i = 0; i < 6; i++) {
            auto shaderProgram = ShaderProgram::create(mDevice);
            
            // 顶点着色器（所有面共享）
            std::string vertexShader = R"(
            #version 450
            layout(location = 0) in vec3 inPosition;
            layout(location = 1) in vec3 inNormal;
            layout(location = 2) in uint inMaterialID;
            
            layout(location = 0) out vec3 fragPosition;
            layout(location = 1) out vec3 fragNormal;
            layout(location = 2) flat out uint fragMaterialID;
            
            layout(binding = 0) uniform UniformBufferObject {
                mat4 model;
                mat4 view;
                mat4 proj;
            } ubo;
            
            void main() {
                vec4 worldPosition = ubo.model * vec4(inPosition, 1.0);
                gl_Position = ubo.proj * ubo.view * worldPosition;
                
                fragPosition = worldPosition.xyz;
                fragNormal = normalize(mat3(ubo.model) * inNormal);
                fragMaterialID = inMaterialID;
            }
            )";
            
            // 检查ShaderProgram是否支持GLSL字符串
            try {
                // 尝试使用GLSL字符串方式
                shaderProgram->addGLSLStringStage(
                    vertexShader,
                    VK_SHADER_STAGE_VERTEX_BIT,
                    "main",
                    {},
                    "VertexShader_CubeFace" + std::to_string(i)
                );
                
                shaderProgram->addGLSLStringStage(
                    fragmentShaders[i],
                    VK_SHADER_STAGE_FRAGMENT_BIT,
                    "main",
                    {},
                    "FragmentShader_CubeFace" + std::to_string(i)
                );
            } catch (const std::exception& e) {
                std::cerr << "Failed to create shader from string: " << e.what() << std::endl;
                std::cerr << "Trying file-based shaders..." << std::endl;
                
                // 回退方案：使用文件
                shaderProgram->addGLSLStage(
                    "assets/shaders/core/shader.vert",
                    VK_SHADER_STAGE_VERTEX_BIT,
                    "main",
                    {},
                    "VertexShader_CubeFace" + std::to_string(i)
                );
                
                // 创建临时文件保存片段着色器
                std::string fragFilename = "temp_frag_" + std::to_string(i) + ".frag";
                std::ofstream fragFile(fragFilename);
                fragFile << fragmentShaders[i];
                fragFile.close();
                
                shaderProgram->addGLSLStage(
                    fragFilename.c_str(),
                    VK_SHADER_STAGE_FRAGMENT_BIT,
                    "main",
                    {},
                    "FragmentShader_CubeFace" + std::to_string(i)
                );
                
                // 删除临时文件
                std::remove(fragFilename.c_str());
            }
            
            mMultiMaterialShaders[i] = shaderProgram;
            
            // 创建ShaderStageComponent并注册
            auto shaderComponent = std::make_shared<ShaderStageComponent>(
                "CubeFaceShader" + std::to_string(i)
            );
            shaderComponent->setShaderProgram(shaderProgram);
            mComponentRegistry->registerComponent(
                "CubeFaceShader" + std::to_string(i), 
                shaderComponent
            );
            
            std::cout << "Created shader for face " << i << std::endl;
        }
    }

    void Application::createMultiplePipelines() {
        std::cout << "Creating multiple pipelines..." << std::endl;
        
        mMultiMaterialPipelines.resize(6);
        
        // 配置顶点输入组件（基于MultiMaterialVertex）
        auto vertexInputComponent = std::dynamic_pointer_cast<VertexInputComponent>(
            mComponentRegistry->getComponent(PipelineComponentType::VERTEX_INPUT, "BasicVertex"));
        
        if (vertexInputComponent) {
            vertexInputComponent->reset();
            
            // 使用VertexArrayBuffer的布局
            if (mMultiMaterialVAO) {
                vertexInputComponent->configureFromVertexBuffer(*mMultiMaterialVAO);
                std::cout << "Configured vertex input from multi-material VAO" << std::endl;
            } else {
                throw std::runtime_error("Multi-material VAO not created");
            }
        }
        
        // 配置视口
        auto viewportComponent = std::dynamic_pointer_cast<ViewportComponent>(
            mComponentRegistry->getComponent(PipelineComponentType::VIEWPORT_STATE, "Fullscreen"));
        
        if (viewportComponent) {
            viewportComponent->reset();
            ViewportScissor viewport(mExtent, true);
            viewportComponent->setViewportScissor(viewport);
        }
        
        // 为每个材质创建管线
        for (int i = 0; i < 6; i++) {
            try {
                // 创建新的PipelineBuilder实例（每个管线需要单独的）
                auto pipelineBuilder = std::make_shared<PipelineBuilder>(
                    mDevice->getHandle(),
                    mComponentRegistry
                );
                
                // 使用不同的光栅化状态来展示多样性
                std::string rasterizationName = "Opaque";
                if (i == 3) {  // 黄色面使用线框模式
                    rasterizationName = "Wireframe";
                }
                
                // 使用不同的混合状态
                std::string blendName = "None";
                if (i == 4) {  // 紫色面使用alpha混合
                    blendName = "Alpha";
                }
                
                // 构建管线
                mMultiMaterialPipelines[i] = pipelineBuilder
                    ->addComponent(PipelineComponentType::SHADER_STAGE, "CubeFaceShader" + std::to_string(i))
                    .addComponent(PipelineComponentType::VERTEX_INPUT, "BasicVertex")
                    .addComponent(PipelineComponentType::INPUT_ASSEMBLY, "TriangleList")
                    .addComponent(PipelineComponentType::VIEWPORT_STATE, "Fullscreen")
                    .addComponent(PipelineComponentType::RASTERIZATION, rasterizationName)
                    .addComponent(PipelineComponentType::MULTISAMPLE, "Default")
                    .addComponent(PipelineComponentType::DEPTH_STENCIL, "Enabled")
                    .addComponent(PipelineComponentType::COLOR_BLEND, blendName)
                    .addComponent(PipelineComponentType::DYNAMIC_STATE, "Basic")
                    .buildGraphicsPipeline(
                        mPipelineLayout->getHandle(),  // 使用原来的布局
                        mRenderPassResult->renderPass->getHandle(),
                        mRenderPassResult->pipelineNameToSubpassIndexMap["MainPipeline"]
                    );
                
                std::cout << "Created pipeline for face " << i << " ("
                          << rasterizationName << ", " << blendName << ")" << std::endl;
            }
            catch (const std::exception& e) {
                std::cerr << "Failed to create pipeline for face " << i << ": " << e.what() << std::endl;
                // 尝试使用默认管线
                try {
                    auto pipelineBuilder = std::make_shared<PipelineBuilder>(
                        mDevice->getHandle(),
                        mComponentRegistry
                    );
                    
                    mMultiMaterialPipelines[i] = pipelineBuilder
                        ->addComponent(PipelineComponentType::SHADER_STAGE, "BasicShader")
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
                    
                    std::cout << "Created default pipeline for face " << i << " as fallback" << std::endl;
                } catch (const std::exception& e2) {
                    std::cerr << "Failed to create fallback pipeline for face " << i << ": " << e2.what() << std::endl;
                    throw;
                }
            }
        }
    }


    void Application::registerDefaultComponents() {
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
        // 这个用于基础管线，可以保留
        mShaderProgram = ShaderProgram::create(mDevice);

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
        RenderPassBuilder builder("MainRenderPass", mDevice);

        // 添加颜色附件
        builder.addColorAttachment(
            "ColorAttachment",
            mRenderer->getBackendAs<VulkanBackend>()->getWindowContext()->getSwapchainFormat(),
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE
        );

        // 添加深度附件
        builder.addDepthAttachment(
            "DepthAttachment",
            Texture::findSupportedDepthFormat(mRenderer->getBackendAs<VulkanBackend>()->getVulkanCore()->getPhysicalDeviceHandle()),
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
        auto extent = mExtent;
        mDepthTexture = Texture::create(
            mDevice,
            Texture::Type::Depth,
            extent,
            mCommandPool
        );
    }

    void Application::createDescriptorManager() {
        mMatrixUniformBuffers.clear();

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            auto matrixUniformBuffer = UniformBuffer::create<UniformBufferObject>(
                mDevice, mCommandPool
            );
            mMatrixUniformBuffers.push_back(matrixUniformBuffer);
        }

        mDescriptorManager = std::make_shared<DescriptorManager>(mDevice);

        // 定义描述符集布局 - set 0只有一个binding用于矩阵
        mDescriptorManager->beginSetLayout(0);
        mDescriptorManager->addUniformBuffer(0, VK_SHADER_STAGE_VERTEX_BIT, 1);
        mDescriptorManager->endSetLayout();

        // 分配描述符集
        mDescriptorManager->allocateSets(MAX_FRAMES_IN_FLIGHT);

        // 为每个帧更新描述符集
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            mDescriptorManager->writeUniformBufferDescriptor<UniformBufferObject>(
                0, 0, i, mMatrixUniformBuffers[i]->getBuffer()
            );
        }
    }

    void Application::createGraphicsPipeline() {
        try {
            // 1. 创建NewPipelineBuilder
            mPipelineBuilder = std::make_shared<PipelineBuilder>(
                mDevice->getHandle(),
                mComponentRegistry
            );

            // 2. 配置顶点输入组件（基于MultiMaterialVertex）
            auto vertexInputComponent = std::dynamic_pointer_cast<VertexInputComponent>(
                mComponentRegistry->getComponent(PipelineComponentType::VERTEX_INPUT, "BasicVertex"));

            if (vertexInputComponent) {
                vertexInputComponent->reset();
                
                if (mMultiMaterialVAO) {
                    vertexInputComponent->configureFromVertexBuffer(*mMultiMaterialVAO);
                    
                    if (!vertexInputComponent->isValid()) {
                        throw std::runtime_error("Vertex input component is invalid");
                    }
                } else {
                    throw std::runtime_error("Multi-material VAO not created");
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

                ViewportScissor viewport(mExtent, true);
                viewportComponent->setViewportScissor(viewport);
            }
            else {
                throw std::runtime_error("Failed to get viewport component");
            }

            // 4. 获取描述符集布局
            auto descriptorSetLayouts = mDescriptorManager->getLayoutHandles();

            // 5. 创建管线布局
            mPipelineLayout = PipelineLayout::create(
                mDevice,
                descriptorSetLayouts
            );

            // 创建基础管线（可选，我们主要用多材质管线）
            try {
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
                std::cout << "Created base graphics pipeline" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Failed to create base graphics pipeline: " << e.what() << std::endl;
                mGraphicsPipeline = VK_NULL_HANDLE; // 这不是关键错误
            }
        }
        catch (const std::exception& e) {
            std::cerr << "ERROR in createGraphicsPipeline: " << e.what() << std::endl;
            throw;  
        }
    }
    void Application::updateUniformBuffer(uint32_t currentFrame) {
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float>(currentTime - startTime).count();

        static glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
        static glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);

        // 更新矩阵
        UniformBufferObject ubo{};

        ubo.model = glm::mat4(1.0f);
        ubo.model = glm::translate(ubo.model, position); 
        ubo.model = glm::rotate(ubo.model, time * glm::radians(45.0f), glm::vec3(0.5f, 1.0f, 0.0f));  
        ubo.model = glm::scale(ubo.model, scale);

        ubo.view = glm::lookAt(
            glm::vec3(2.0f, 2.0f, 2.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f)
        );
        ubo.proj = glm::perspective(
            glm::radians(45.0f),
            mWindow->getAspectRatio(),
            0.1f,
            100.0f
        );

        // 如果是Vulkan坐标系，需要反转Y轴
        // ubo.proj[1][1] *= -1; 

        mMatrixUniformBuffers[currentFrame]->updateData(&ubo, sizeof(ubo));
    }

    void Application::recordCommandBuffer(RenderContext& context, uint32_t imageIndex) {
        auto renderpass = mRenderPassResult->renderPass->getHandle();
        auto framebuffer = mSwapchainFramebuffers[imageIndex];

        passBeginInfo.reset()
            .addClearColor({0.08f, 0.08f, 0.12f, 1.0f})
            .addClearDepth({1.0f, 0})
            .update(renderpass, framebuffer, mExtent);

        context.beginRenderPass(&passBeginInfo.getRenderPassBeginInfo(), VK_SUBPASS_CONTENTS_INLINE);

        auto viewport = std::dynamic_pointer_cast<ViewportComponent>(
            mComponentRegistry->getComponent(PipelineComponentType::VIEWPORT_STATE, "Fullscreen")
        );
        context.setViewport(viewport->getViewports()[0]);
        context.setScissor(viewport->getScissors()[0]);

        // 绑定顶点和索引缓冲区
        if (mMultiMaterialVAO && mMultiMaterialIBO) {
            auto VBO = mMultiMaterialVAO->getBufferHandles();
            context.bindVertexBuffers(VBO);
            auto IBO = mMultiMaterialIBO->getBuffer();
            context.bindIndexBuffer(IBO);

            uint32_t frameIndex = mRenderer->getBackendAs<VulkanBackend>()->getCurrentFrameIndex();
            VkDescriptorSet descriptorSet = mDescriptorManager->getDescriptorSet(0, frameIndex);
            
            // 绘制立方体，每个面使用不同的管线
            for (int face = 0; face < 6; face++) {
                if (face < mMultiMaterialPipelines.size() && 
                    mMultiMaterialPipelines[face] != VK_NULL_HANDLE) {
                    
                    // 绑定这个面的管线
                    context.bindGraphicsPipeline(mMultiMaterialPipelines[face]);
                    
                    // 绑定描述符集（矩阵）
                    context.bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, 
                                            descriptorSet, 0, 
                                            mPipelineLayout->getHandle());

                    // 绘制这个面（2个三角形 = 6个索引）
                    uint32_t firstIndex = face * 6;
                    uint32_t indexCount = 6;
                    
                    context.drawIndexed(indexCount, 1, firstIndex, 0, 0);
                } else {
                    std::cerr << "Warning: Pipeline for face " << face << " is not valid" << std::endl;
                }
            }
        } else {
            std::cerr << "Error: Multi-material buffers not available" << std::endl;
        }

        context.endRenderPass();
    }

    void Application::drawFrame() {
        mRenderer->getBackendAs<VulkanBackend>()->beginFrame();
        uint32_t frameIndex = mRenderer->getBackendAs<VulkanBackend>()->getCurrentFrameIndex();
        uint32_t imageIndex = mRenderer->getBackendAs<VulkanBackend>()->getCurrentImageIndex();
        updateUniformBuffer(frameIndex);
        if (auto* ctx = mRenderer->getBackendAs<VulkanBackend>()->getCurrentFrameContext()) {
            recordCommandBuffer(*ctx->renderContext, imageIndex);
        }
        mRenderer->getBackendAs<VulkanBackend>()->submitFrame();
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

        vkDeviceWaitIdle(mDevice->getHandle());
    }

    void Application::cleanupSwapchain() {
        for (auto framebuffer : mSwapchainFramebuffers) {
            vkDestroyFramebuffer(mDevice->getHandle(), framebuffer, nullptr);
        }
        mSwapchainFramebuffers.clear();
    }

    void Application::recreateSwapchain() {
        vkDeviceWaitIdle(mDevice->getHandle());

        cleanupSwapchain();

        mRenderer->onSwapchainRecreated();

        createDepthTexture();

        auto viewportComponent = std::dynamic_pointer_cast<ViewportComponent>(
            mComponentRegistry->getComponent(PipelineComponentType::VIEWPORT_STATE, "Fullscreen"));

        if (viewportComponent) {
            viewportComponent->reset();

            ViewportScissor viewport(mExtent, true);
            viewportComponent->setViewportScissor(viewport);
        }
        //createFramebuffers();
        mRenderer->createFramebuffers(mSwapchainFramebuffers,mDepthTexture->getImageView(),mRenderPassResult->renderPass->getHandle());
    }

    void Application::cleanup() {
        cleanupSwapchain();

        // 清理多材质管线
        for (auto& pipeline : mMultiMaterialPipelines) {
            if (pipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(mDevice->getHandle(), pipeline, nullptr);
            }
        }
        mMultiMaterialPipelines.clear();

        // 清理基础管线
        if (mGraphicsPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(mDevice->getHandle(), mGraphicsPipeline, nullptr);
            mGraphicsPipeline = VK_NULL_HANDLE;
        }
        
        if (mPipelineLayout) {
            mPipelineLayout.reset();
        }

        // 清理多材质资源
        mMultiMaterialVAO.reset();
        mMultiMaterialIBO.reset();
        
        for (auto& program : mMultiMaterialShaders) {
            program.reset();
        }
        mMultiMaterialShaders.clear();

        // 清理uniform buffers
        mMatrixUniformBuffers.clear();
        mColorUniformBuffers.clear();

        // 清理材质颜色buffers
        for (auto& frameBuffers : mMaterialColorBuffers) {
            for (auto& buffer : frameBuffers) {
                buffer.reset();
            }
        }
        mMaterialColorBuffers.clear();

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
        mWindow.reset();
    }

    Application::~Application() {
        cleanup();
    }

} // namespace StarryEngine