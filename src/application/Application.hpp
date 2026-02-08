#pragma once
#ifdef __linux__
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <dlfcn.h>
#endif

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

                layout(location = 0) out vec3 fragColor;

                vec2 positions[3] = vec2[](
                    vec2(0.0, -0.5),
                    vec2(0.5, 0.5),
                    vec2(-0.5, 0.5)
                );

                vec3 colors[3] = vec3[](
                    vec3(1.0, 0.0, 0.0),
                    vec3(0.0, 1.0, 0.0),
                    vec3(0.0, 0.0, 1.0)
                );

                void main() {
                    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
                    fragColor = colors[gl_VertexIndex];
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

                layout(location = 0) in vec3 fragColor;

                layout(location = 0) out vec4 outColor;

                void main() {
                    outColor = vec4(fragColor, 1.0);
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

        void createBuffer() {
            // 创建顶点缓冲区描述
            RHI::BufferDesc bufferDesc;
            bufferDesc.size = 1024;  // 1KB
            bufferDesc.type = RHI::BufferType::Vertex;
            bufferDesc.memoryType = RHI::MemoryType::CPU_To_GPU;
            bufferDesc.stride = sizeof(float) * 3;  // vec3 position
            bufferDesc.allowUpdate = true;
            bufferDesc.debugName = "MyVertexBuffer";

            // 通过资源管理器创建缓冲区
            mVertexBufferHandle = m_rhi->createBuffer(bufferDesc);

            if (!mVertexBufferHandle.isValid()) {
                std::cerr << "Failed to create vertex buffer!" << std::endl;
                return;
            }

            std::cout << "Vertex buffer created successfully!" << std::endl;

            if (mVertexBufferHandle.isValid()) {
                StarryEngine::RHI::RHIBuffer* mVertexBuffer = m_rhi->getBuffer(mVertexBufferHandle);
                if (mVertexBuffer) {
                    // 顶点数据
                    std::vector<float> vertices = {
                        -0.5f, -0.5f, 0.0f,
                         0.5f, -0.5f, 0.0f,
                         0.0f,  0.5f, 0.0f
                    };

                    // 使用update方法填充数据
                    mVertexBuffer->update(vertices.data(), vertices.size() * sizeof(float));

                    std::cout << "Vertex data uploaded successfully!" << std::endl;
                }
            }
        }

        void createRenderPass() {

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

		StarryEngine::RHI::BufferHandle mVertexBufferHandle = RHI::BufferHandle::Null();
		StarryEngine::RHI::RHIBuffer* mVertexBuffer = nullptr;
    };

} // namespace StarryEngine