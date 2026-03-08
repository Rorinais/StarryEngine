#pragma once
#include "IRenderpass.hpp"
#include "../../subpassRecorder/GBufferRecorder.hpp"

namespace StarryEngine::RenderGraph {

    class PostProcessPass : public IRenderPass {
    public:
        PostProcessPass(std::shared_ptr<RHI::ResourceManager> resMgr,
            RHI::DescriptorPoolHandle descriptorPool)
            : m_resMgr(resMgr), m_descriptorPool(descriptorPool) {
            createRecorder();
        }
        ~PostProcessPass() override = default;

        // 实现 IRenderPass 接口
        void setup(RenderGraph& renderGraph, TextureId input, TextureId output) override {
            m_renderGraph = &renderGraph;
            auto postPass = renderGraph.addPassNode("PostPass");
            postPass->setRenderArea(width, height);
            postPass->addColorOutput(output)
                .setClearColor({ 0.0f, 0.0f, 0.0f, 1.0f })
                .setFinalLayout(RHI::ImageLayout::PresentSrc);

            postPass->addInput(input)
                .setInitialLayout(RHI::ImageLayout::ShaderReadOnly);

            auto postSubpass = postPass->addSubpassProxy("PostSubpass")
                .addColorAttachment(output)
                .addInputAttachment(input)
                .setPipelineName("PostPipeline")
                .setRecorder(m_postRecorder.get())
                .setPipelineDescription(
                    renderGraph.createBasePipelineDesc(
                        m_postRecorder->getVertexShader(),
                        m_postRecorder->getFragmentShader(),
                        {},
                        m_postRecorder->getPipelineLayout(),
                        width, height, 1, 1.0f,
                        RHI::PrimitiveTopology::TriangleList,
                        RHI::CullMode::None,
                        false, false)
                );
        }

        std::shared_ptr<PostProcessRecorder> getGridRecorder() const { return m_postRecorder; }

        void updateInputAttachment(TextureId texture, RHI::ImageLayout layout) {
            RHI::TextureHandle intermediatePhysAfter = m_renderGraph->getPhysicalTextureHandle(texture);
            if (intermediatePhysAfter.isValid()) {
                m_postRecorder->updateInputAttachment(0, intermediatePhysAfter, layout);
            }
            else {
                std::cerr << "Failed to get valid intermediate texture handle after compile!" << std::endl;
            }
        }

    private:
        void createRecorder() {
            m_postRecorder = std::make_shared<PostProcessRecorder>(m_resMgr);

            // 全屏三角形顶点着色器
            std::string fullscreenVS = R"(
            #version 450
            layout(location = 0) out vec2 outUV;
            void main() {
                const vec3 positions[3] = vec3[](
                    vec3(-1.0, -1.0, 0.0),
                    vec3( 3.0, -1.0, 0.0),
                    vec3(-1.0,  3.0, 0.0)
                );
                gl_Position = vec4(positions[gl_VertexIndex], 1.0);
                outUV = positions[gl_VertexIndex].xy * 0.5 + 0.5;
            }
        )";
            m_postRecorder->setVertexShader(fullscreenVS, "PostVS");

            // 后处理片元着色器（使用输入附件）
            std::string postFS = R"(
            #version 450
            layout(location = 0) in vec2 inUV;
            layout(location = 0) out vec4 outColor;
            layout(input_attachment_index = 0, binding = 0) uniform subpassInput inputColor;

            void main() {
                vec2 center = vec2(0.5, 0.5);
                float dist = distance(inUV, center);
                float innerRadius = 0.0;
                float outerRadius = 0.5;
                float t = clamp((dist - innerRadius) / (outerRadius - innerRadius), 0.0, 1.0);
                vec3 originalColor = subpassLoad(inputColor).rgb;
                vec3 invertedColor = 1.0 - originalColor;
                vec3 finalColor = mix(invertedColor, originalColor, t);
                outColor = vec4(finalColor, 1.0);
            }
        )";
            m_postRecorder->setFragmentShader(postFS, "PostFS");

            // 添加输入附件绑定
            m_postRecorder->addInputAttachmentBinding(0, RHI::ShaderStage::Fragment);
            m_postRecorder->createDescriptorSetLayout();
            m_postRecorder->createPipelineLayout("PostPipelineLayout");
            m_postRecorder->allocateDescriptorSet(m_descriptorPool, 0);
        }

        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        RHI::DescriptorPoolHandle m_descriptorPool;
        std::shared_ptr<PostProcessRecorder> m_postRecorder;

        RenderGraph* m_renderGraph;
    };

}