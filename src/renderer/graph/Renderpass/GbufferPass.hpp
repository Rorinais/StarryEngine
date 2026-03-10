#pragma once
#include "IRenderpass.hpp"
#include "../../subpassRecorder/GBufferRecorder.hpp"

namespace StarryEngine::RenderGraph {


    class GbufferPass : public IRenderPass {
    public:
        GbufferPass(std::shared_ptr<RHI::ResourceManager> resMgr,
            RHI::DescriptorPoolHandle descriptorPool)
            : m_resMgr(resMgr), m_descriptorPool(descriptorPool) {
            createGbufferRecorder(); // 创建 GBufferRecorder
            createGridRecorder();    // 创建 GridRecorder

            // 再设置几何体数据
            createCubeGeometry();    // 设置立方体几何体到 m_gbufferRecorder
            createGridGeometry();    // 设置网格几何体到 m_gridRecorder
        }

        // 实现 IRenderPass 接口
        void setup(RenderGraph& renderGraph, TextureId input, TextureId output,
            RHI::ImageLayout depthInitial,
            RHI::ImageLayout depthFinal,
            RHI::ImageLayout colorInitial,
            RHI::ImageLayout colorFinal) {
            auto* mainPass = renderGraph.addPassNode("MainPass");
            mainPass->setRenderArea(width, height);
            mainPass->addColorOutput(output)
                .setClearColor({ 0.05f, 0.05f, 0.05f, 1.0f })
                .setInitialLayout(colorInitial)
                .setFinalLayout(colorFinal);
            mainPass->addDepthOutput(input)
                .setInitialLayout(depthInitial)
                .setFinalLayout(depthFinal)
                .setClearDepth(1.0f);

            // 子通道 0：网格
            auto gridSubpass = mainPass->addSubpassProxy("GridSubpass")
                .addColorAttachment(output)
                .addDepthStencilAttachment(input)
                .setPipelineName("GridPipeline")
                .setRecorder(m_gridRecorder.get())
                .setPipelineDescription(
                    renderGraph.createBasePipelineDesc(
                        m_gridRecorder->getVertexShader(),
                        m_gridRecorder->getFragmentShader(),
                        m_gridRecorder->getVertexInputState(),
                        m_gridRecorder->getPipelineLayout(),
                        width, height, 1, 1.0f,
                        RHI::PrimitiveTopology::LineList)
                );

            // 子通道 1：几何体
            auto geomSubpass = mainPass->addSubpassProxy("GeomSubpass")
                .addColorAttachment(output)
                .addDepthStencilAttachment(input)
                .setPipelineName("GeomPipeline")
                .setRecorder(m_gbufferRecorder.get())
                .setPipelineDescription(
                    renderGraph.createBasePipelineDesc(
                        m_gbufferRecorder->getVertexShader(),
                        m_gbufferRecorder->getFragmentShader(),
                        m_gbufferRecorder->getVertexInputState(),
                        m_gbufferRecorder->getPipelineLayout(),
                        width, height)
                );
        }

        std::shared_ptr<GBufferRecorder> getGbufferRecorder() const { return m_gbufferRecorder; }
        std::shared_ptr<GridRecorder> getGridRecorder() const { return m_gridRecorder; }

        RHI::BufferHandle getUniformBufferHandle() const { return m_uniformBufferHandle; }
        RHI::BufferHandle getGridUniformBufferHandle() const { return m_gridUniformBufferHandle; }

        void update(const Uniforms& data)override {
            auto* uniformBuffer = m_resMgr->getBuffer(m_uniformBufferHandle);
            uniformBuffer->update(&data, sizeof(data), 0);

            Uniforms gridUbo = { glm::mat4(1.0f), data.view, data.proj };
            auto* gridUniformBuffer = m_resMgr->getBuffer(m_gridUniformBufferHandle);
            gridUniformBuffer->update(&gridUbo, sizeof(gridUbo), 0);
        
        }

    private:
        void createGbufferRecorder() {
            m_gbufferRecorder = std::make_shared<GBufferRecorder>(m_resMgr);

            // 顶点着色器
            std::string vsCode = R"(
                #version 450
                layout(location = 0) in vec3 inPosition;
                layout(location = 1) in vec3 inColor;
                layout(location = 2) in vec2 inTexCoord;
                layout(location = 0) out vec3 fragColor;
                layout(location = 1) out vec2 fragTexCoord;
                layout(binding = 0) uniform UniformBufferObject {
                    mat4 model;
                    mat4 view;
                    mat4 proj;
                } ubo;
                void main() {
                    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
                    fragColor = inColor;
                    fragTexCoord = inTexCoord;
                }
            )";
            m_gbufferRecorder->setVertexShader(vsCode, "GBufferVS");

            // 片段着色器
            std::string fsCode = R"(
                #version 450
                layout(location = 0) in vec3 fragColor;
                layout(location = 1) in vec2 fragTexCoord;
                layout(location = 0) out vec4 outColor;
                layout(binding = 1) uniform sampler2D texSampler;
                void main() {
                    outColor = texture(texSampler, fragTexCoord) * vec4(fragColor, 1.0);
                }
            )";
            m_gbufferRecorder->setFragmentShader(fsCode, "GBufferFS");

            // 创建 Uniform 缓冲区
            m_uniformBufferHandle = m_gbufferRecorder->createAndAddUniformBuffer(sizeof(Uniforms), 0, "GBufferUniformBuffer");

            // 添加纹理
            m_gbufferRecorder->addTexture("C:\\Users\\41384\\Desktop\\Snipaste.png",
                RHI::Format::RGBA8_UNorm,
                "DiffuseTexture",
                1);

            // 创建描述符集布局和管道布局
            m_gbufferRecorder->createDescriptorSetLayout();
            m_gbufferRecorder->createPipelineLayout("GBufferPipelineLayout");

            // 分配并更新描述符集
            m_gbufferRecorder->allocateDescriptorSet(m_descriptorPool);
            m_gbufferRecorder->updateDescriptorSet();
        }

        void createCubeGeometry() {

            std::vector<float> vertices = {
                // 背面 (z = -0.5)
                -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,  0.0f, 0.0f,
                 0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,  1.0f, 0.0f,
                 0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f,
                -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,  0.0f, 1.0f,
                // 正面 (z = 0.5)
                -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,  0.0f, 0.0f,
                 0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f,  1.0f, 0.0f,
                 0.5f,  0.5f,  0.5f,  0.5f, 0.5f, 1.0f,  1.0f, 1.0f,
                -0.5f,  0.5f,  0.5f,  1.0f, 0.5f, 0.5f,  0.0f, 1.0f,
                // 左面 (x = -0.5)
                -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,  0.0f, 0.0f,
                -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,  1.0f, 0.0f,
                -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,  1.0f, 1.0f,
                -0.5f,  0.5f,  0.5f,  1.0f, 0.5f, 0.5f,  0.0f, 1.0f,
                // 右面 (x = 0.5)
                 0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f,
                 0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f,  1.0f, 0.0f,
                 0.5f,  0.5f,  0.5f,  0.5f, 0.5f, 1.0f,  1.0f, 1.0f,
                 0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,  0.0f, 1.0f,
                 // 顶面 (y = 0.5)
                 -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f,
                  0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,  1.0f, 0.0f,
                  0.5f,  0.5f,  0.5f,  0.5f, 0.5f, 1.0f,  1.0f, 1.0f,
                 -0.5f,  0.5f,  0.5f,  1.0f, 0.5f, 0.5f,  0.0f, 1.0f,
                 // 底面 (y = -0.5)
                 -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,  0.0f, 0.0f,
                  0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f,  1.0f, 0.0f,
                  0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,  1.0f, 1.0f,
                 -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,  0.0f, 1.0f
            };
            std::vector<uint32_t> indices = {
                0,1,2, 2,3,0,      // 背面
                4,5,6, 6,7,4,      // 正面
                8,9,10, 10,11,8,   // 左面
                12,13,14, 14,15,12,// 右面
                16,17,18, 18,19,16,// 顶面
                20,21,22, 22,23,20 // 底面
            };

            VertexLayout layout;
            layout.addBinding(0, 8 * sizeof(float), RHI::VertexInputRate::PerVertex)
                .addAttribute(0, 0, RHI::Format::RGB32_Float)   // 位置
                .addAttribute(1, 0, RHI::Format::RGB32_Float)   // 颜色
                .addAttribute(2, 0, RHI::Format::RG32_Float);   // 纹理坐标


            m_gbufferRecorder->setVertexBuffer(0, vertices, layout, "posBuffer");
            m_gbufferRecorder->setIndexBuffer(indices, "CubeIndexBuffer");

        }

        void createGridRecorder() {
            m_gridRecorder = std::make_shared<GridRecorder>(m_resMgr);

            // 顶点着色器
            std::string vsCode = R"(
                #version 450
                layout(location = 0) in vec3 inPosition;
                layout(location = 1) in vec3 inColor;
                layout(location = 0) out vec3 fragColor;
                layout(binding = 0) uniform UniformBufferObject {
                    mat4 model;
                    mat4 view;
                    mat4 proj;
                } ubo;
                void main() {
                    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
                    fragColor = inColor;
                }
            )";
            m_gridRecorder->setVertexShader(vsCode, "GridVS");

            // 片段着色器
            std::string fsCode = R"(
                #version 450
                layout(location = 0) in vec3 fragColor;
                layout(location = 0) out vec4 outColor;
                void main() {
                    outColor = vec4(fragColor, 0.5);
                }
            )";
            m_gridRecorder->setFragmentShader(fsCode, "GridFS");

            // 创建 Uniform 缓冲区
            m_gridUniformBufferHandle = m_gridRecorder->createAndAddUniformBuffer(sizeof(Uniforms), 0, "GridUniformBuffer");

            // 创建描述符集布局和管道布局
            m_gridRecorder->createDescriptorSetLayout();
            m_gridRecorder->createPipelineLayout("GridPipelineLayout");

            // 分配并更新描述符集
            m_gridRecorder->allocateDescriptorSet(m_descriptorPool);
            m_gridRecorder->updateDescriptorSet();
        }

        void createGridGeometry() {
            // 生成网格数据（同之前 Application 中的 createGrid）
            std::vector<float> vertices;
            std::vector<uint32_t> indices;
            const float size = 50.0f;
            const int divisions = 50;
            const float step = size / divisions;
            const float half = size * 0.5f;

            const glm::vec3 colorXAxis(1.0f, 0.0f, 0.0f);
            const glm::vec3 colorYAxis(0.0f, 1.0f, 0.0f);
            const glm::vec3 colorZAxis(0.0f, 0.0f, 1.0f);
            const glm::vec3 colorLine(0.4f, 0.4f, 0.4f);

            auto addVertex = [&](float x, float y, float z, const glm::vec3& col) {
                vertices.push_back(x);
                vertices.push_back(y);
                vertices.push_back(z);
                vertices.push_back(col.r);
                vertices.push_back(col.g);
                vertices.push_back(col.b);
                };

            // X 轴方向线条
            for (int i = 0; i <= divisions; ++i) {
                float z = -half + i * step;
                bool isXAxis = (std::abs(z) < 0.001f);
                addVertex(-half, 0.0f, z, isXAxis ? colorXAxis : colorLine);
                addVertex(half, 0.0f, z, isXAxis ? colorXAxis : colorLine);
            }

            // Z 轴方向线条
            for (int i = 0; i <= divisions; ++i) {
                float x = -half + i * step;
                bool isZAxis = (std::abs(x) < 0.001f);
                addVertex(x, 0.0f, -half, isZAxis ? colorZAxis : colorLine);
                addVertex(x, 0.0f, half, isZAxis ? colorZAxis : colorLine);
            }

            // Y 轴线
            addVertex(0.0f, -half, 0.0f, colorYAxis);
            addVertex(0.0f, half, 0.0f, colorYAxis);

            // 生成索引：每两个连续顶点构成一条线段
            uint32_t vertexCount = static_cast<uint32_t>(vertices.size() / 6);
            for (uint32_t i = 0; i < vertexCount; i += 2) {
                indices.push_back(i);
                indices.push_back(i + 1);
            }

            VertexLayout layout;
            layout.addBinding(0, 6 * sizeof(float), RHI::VertexInputRate::PerVertex)
                .addAttribute(0, 0, RHI::Format::RGB32_Float)   // 位置
                .addAttribute(1, 0, RHI::Format::RGB32_Float);  // 颜色

            m_gridRecorder->setVertexBuffer(0, vertices, layout, "GridVertexBuffer");
            m_gridRecorder->setIndexBuffer(indices, "GridIndexBuffer");
        }

        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        RHI::DescriptorPoolHandle m_descriptorPool;
        std::shared_ptr<GBufferRecorder> m_gbufferRecorder;
        std::shared_ptr<GridRecorder> m_gridRecorder;
        RHI::BufferHandle m_uniformBufferHandle;
        RHI::BufferHandle m_gridUniformBufferHandle;
    };

} // namespace StarryEngine::RenderGraph