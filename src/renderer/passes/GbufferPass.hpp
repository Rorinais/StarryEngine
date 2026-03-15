#pragma once
#include "IRenderpass.hpp"
#include "../subpassRecorder/GBufferRecorder.hpp"
#include "../../assets/loader/ModelLoader.hpp"
#include "../../logging/Logger.hpp"

namespace StarryEngine::RenderGraph {


    class GbufferPass : public IRenderPass {
    public:
        GbufferPass(std::shared_ptr<RHI::ResourceManager> resMgr,
            RHI::DescriptorPoolHandle descriptorPool)
            : m_resMgr(resMgr), m_descriptorPool(descriptorPool) {
            createRecorder();
        }

        void setDrawItems(const std::vector<Scene::DrawItem>& items) {
            m_recorder->setDrawItems(items);
        }

        void setVertexLayout(RHI::VertexInputState vertex) {
            m_vertexLayout = vertex;
        }

        void setup(RenderGraph& renderGraph, TextureId input, TextureId output,
            RHI::ImageLayout depthInitial, RHI::ImageLayout depthFinal,
            RHI::ImageLayout colorInitial, RHI::ImageLayout colorFinal) {
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

            auto geomSubpass = mainPass->addSubpassProxy("GeomSubpass")
                .addColorAttachment(output)
                .addDepthStencilAttachment(input)
                .setPipelineName("GeomPipeline")
                .setRecorder(m_recorder.get())
                .setPipelineDescription(
                    renderGraph.createBasePipelineDesc(
                        m_recorder->getVertexShader(),
                        m_recorder->getFragmentShader(),
                        m_vertexLayout,
                        m_recorder->getPipelineLayout(),
                        width, height, 1, 1.0f)
                );
        }


        std::shared_ptr<MeshDrawRecorder> getMeshDrawRecorder() const { return m_recorder; }

        RHI::BufferHandle getUniformBufferHandle() const { return m_uniformBufferHandle; }

    private:
        void createRecorder() {
            m_recorder = std::make_shared<MeshDrawRecorder>(m_resMgr);

            // 顶点着色器
            std::string vsCode = R"(
                #version 450
                layout(location = 0) in vec3 inPosition;
                layout(location = 1) in vec3 inColor;
                layout(location = 2) in vec2 inTexCoord;
                layout(location = 0) out vec3 fragColor;
                layout(location = 1) out vec2 fragTexCoord;
                layout(set = 0, binding = 0) uniform UniformBufferObject {
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
            m_recorder->setVertexShader(vsCode, "GBufferVS");

            // 片段着色器（使用顶点颜色，暂时忽略纹理）
            std::string fsCode = R"(
                #version 450
                layout(location = 0) in vec3 fragColor;
                layout(location = 1) in vec2 fragTexCoord;
                layout(location = 0) out vec4 outColor;
                void main() {
                    outColor = vec4(1.0, 0.0, 0.0, 1.0);
                }
            )";
            m_recorder->setFragmentShader(fsCode, "GBufferFS");

            m_uniformBufferHandle = m_recorder->createAndAddUniformBuffer(sizeof(Assets::Uniforms), 0, "GBufferUniformBuffer");

            // 添加绑定信息
            m_recorder->addBinding(0, RHI::DescriptorType::UniformBuffer, 1,
                RHI::ShaderStage::Vertex | RHI::ShaderStage::Fragment);

            // 创建管线布局
            auto layout = m_recorder->createPipelineLayout("GBufferPipelineLayout");
            if (!layout.isValid()) {
                LOG_ERROR("Pipeline layout creation failed!");
            }

            // 分配描述符集
            bool allocated = m_recorder->allocateDescriptorSet(m_descriptorPool);

            m_recorder->updateDescriptorSet();
        }

        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        RHI::DescriptorPoolHandle m_descriptorPool;
        std::shared_ptr<MeshDrawRecorder> m_recorder;
        RHI::BufferHandle m_uniformBufferHandle;

        std::shared_ptr<Assets::Geometry> mGeometry;
        RHI::VertexInputState m_vertexLayout;
    };

} // namespace StarryEngine::RenderGraph