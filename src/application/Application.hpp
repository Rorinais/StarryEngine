#pragma once
#include "../base.hpp"
#include "Window.hpp"
#include "../renderer//backend/VulkanRHI.hpp"

#include "../renderer/interface/RHI_TYPES.hpp"
#include "../renderer/interface/RHI_STRUCTS_CONFIG.hpp"
#include "../renderer/interface/RHI_STRUCTS_DESC.hpp"


namespace StarryEngine {

    class Application {
    public:
        Application();
        ~Application();

        void run();

        void createShaderProgram() {
            RHI::ShaderModuleDesc vertexshaderDesc;
            vertexshaderDesc.stage = RHI::ShaderStage::Vertex;
            vertexshaderDesc.sourcecode = R"(
            #version 450
            #extension GL_KHR_vulkan_glsl : enable

            layout(location = 0) in vec3 inPosition;

            layout(location = 0) out vec3 fragTexCoord;

            void main() {
                gl_Position = vec4(inPosition, 1.0);
                fragTexCoord = inPosition;
            }
            )";

            vertexshaderDesc.includePaths = {};
            vertexshaderDesc.debugName = "vertexShader";
            shaderHandles.push_back(m_rhi->createShaderHandle(vertexshaderDesc));

            RHI::ShaderModuleDesc fragmentshaderDesc;
            fragmentshaderDesc.stage = RHI::ShaderStage::Fragment;
            fragmentshaderDesc.sourcecode = R"(
            #version 450
            #extension GL_KHR_vulkan_glsl : enable

            layout(location = 0) in vec3 fragTexCoord;

            layout(location = 0) out vec4 outColor;

            void main() {
                outColor = vec4(fragTexCoord, 1.0);
            }
            )";
            fragmentshaderDesc.includePaths = {};
            fragmentshaderDesc.debugName = "fragmentShader";
            shaderHandles.push_back(m_rhi->createShaderHandle(fragmentshaderDesc));

        }

        void createPipline() {
            RHI::GraphicsPipelineDesc desc;

            desc.vertexShader =shaderHandles[0];
            desc.fragmentShader = shaderHandles[1];

            RHI::PipelineLayoutDesc layoutDesc;
            layoutDesc.descriptorSets = {};
            layoutDesc.pushConstants = {};
            desc.layoutDesc = layoutDesc;

            RHI::VertexLayout vertexLayout;
            RHI::VertexAttribute vertexAttribute;
            vertexAttribute.location = 0;
            vertexAttribute.binding = 0;
            vertexAttribute.format = RHI::Format::RGBA32_Float;
            vertexAttribute.offset = 0;
            vertexLayout.attributes.push_back(vertexAttribute);
            desc.vertexLayout = vertexLayout;

            desc.topology = RHI::PrimitiveTopology::TriangleList;
            desc.primitiveRestartEnable = false;

            RHI::RasterizerState rasterizerState;
            rasterizerState.cullMode = RHI::CullMode::None;
            rasterizerState.frontFace = RHI::FrontFace::CounterClockwise;
            rasterizerState.lineWidth = 1.0f;
            desc.rasterizer = rasterizerState;

            RHI::DepthStencilState depthStencilState;
            depthStencilState.depthTestEnable = true;
            depthStencilState.depthWriteEnable = true;
            depthStencilState.depthCompareOp = RHI::CompareOp::Less;
            desc.depthStencil = depthStencilState;

            RHI::ColorBlendState colorBlendState;
            RHI::BlendAttachmentState attachment;
            colorBlendState.attachments.push_back(attachment);
    
            desc.dynamicStates.push_back("Viewport");
            desc.dynamicStates.push_back("Scissor");

            desc.renderTargetFormats.push_back(RHI::Format::BGRA8_sRGB);
            desc.depthStencilFormat = RHI::Format::D32_Float;

            desc.renderPass = nullptr;
            desc.subpass = 0;
            
            m_rhi->createGraphicsPipeline(desc);
        }
    private:
        // 窗口相关
        uint32_t m_width = 800;
        uint32_t m_height = 600;
        const char* m_title = "StarryEngine";
        const char* m_icon_path = "assets/icons/window_icon.png";
        Window::Ptr m_window;
        bool mFramebufferResized = false;

        std::shared_ptr<VulkanRHI> m_rhi;

        std::vector<RHI::ShaderHandle> shaderHandles;
    };

} // namespace StarryEngine