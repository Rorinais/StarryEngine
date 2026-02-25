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
    static constexpr uint32_t SUBPASS_EXTERNAL = ~0U;
    static constexpr uint32_t SUBPASS_MAX_ENUM = 0x7FFFFFFF;

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
            // 1. 定义附件
            RHI::AttachmentDesc colorAttachment{
                .format = RHI::Format::BGRA8_sRGB,
                .sampleCount = 1,
                .loadOp = RHI::AttachmentLoadOp::Clear,
                .storeOp = RHI::AttachmentStoreOp::Store,
                .stencilLoadOp = RHI::AttachmentLoadOp::DontCare,
                .stencilStoreOp = RHI::AttachmentStoreOp::DontCare,
                .initialLayout = RHI::ImageLayout::Undefined,
                .finalLayout = RHI::ImageLayout::PresentSrc
            };

            RHI::AttachmentDesc depthAttachment{
                .format = m_depthFormat,
                .sampleCount = 1,
                .loadOp = RHI::AttachmentLoadOp::Clear,
                .storeOp = RHI::AttachmentStoreOp::DontCare,
                .stencilLoadOp = RHI::AttachmentLoadOp::Clear,
                .stencilStoreOp = RHI::AttachmentStoreOp::DontCare,
                .initialLayout = RHI::ImageLayout::Undefined,
                .finalLayout = RHI::ImageLayout::DepthStencilAttachment
            };

            // 2. 定义附件引用
            RHI::AttachmentReference colorAttachmentRef{
                .attachment = 0,
                .layout = RHI::ImageLayout::ColorAttachment
            };

            constexpr uint32_t ATTACHMENT_UNUSED = std::numeric_limits<uint32_t>::max();
            RHI::AttachmentReference depthAttachmentRef{
                .attachment = 1,
                .layout = RHI::ImageLayout::DepthStencilAttachment
            };

            //RHI::AttachmentReference depthAttachmentRef{
            //    .attachment = ATTACHMENT_UNUSED,
            //    .layout = RHI::ImageLayout::Undefined
            //};

            // 3. 定义子通道
            RHI::SubpassDesc subpass{
                .inputAttachments = {},
                .colorAttachments = { colorAttachmentRef },
                .resolveAttachments = {},
                .depthStencilAttachment = depthAttachmentRef,
                .preserveAttachments = {}
            };

            // 4. 定义依赖（使用位运算）
            RHI::SubpassDependency dependency1{
                .srcSubpass = SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::BottomOfPipe),
                .dstStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::ColorAttachmentOutput) |
                                static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::EarlyFragmentTests),
                .srcAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::None),
                .dstAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::ColorAttachmentWrite) |
                                 static_cast<RHI::AccessFlags>(RHI::AccessFlag::DepthStencilAttachmentWrite),
                .byRegion = true
            };

            //RHI::SubpassDependency dependency1{
            //    .srcSubpass = SUBPASS_EXTERNAL,
            //    .dstSubpass = 0,
            //    .srcStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::ColorAttachmentOutput), // 等待之前的颜色输出完成
            //    .dstStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::ColorAttachmentOutput) |
            //                    static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::EarlyFragmentTests),
            //    .srcAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::ColorAttachmentWrite), // 之前的写入需要可见
            //    .dstAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::ColorAttachmentWrite), // 后续将写入颜色附件
            //    .byRegion = true
            //};

            RHI::SubpassDependency dependency2{
                .srcSubpass = 0,
                .dstSubpass = SUBPASS_EXTERNAL,
                .srcStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::ColorAttachmentOutput) |
                                static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::LateFragmentTests),
                .dstStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::BottomOfPipe),
                .srcAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::ColorAttachmentWrite) |
                                 static_cast<RHI::AccessFlags>(RHI::AccessFlag::DepthStencilAttachmentWrite),
                .dstAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::None),
                .byRegion = true
            };

            // 5. 组装渲染通道
            RHI::RenderPassDesc renderPassDesc{
                .attachments = { colorAttachment, depthAttachment },
                .subpasses = { subpass },
                .dependencies = { dependency1, dependency2 },
                .debugName = "MainPass"
            };

            //RHI::RenderPassDesc renderPassDesc{
            //    .attachments = { colorAttachment },
            //    .subpasses = { subpass },
            //    .dependencies = { dependency1 },
            //    .debugName = "MainPass"
            //};

            std::cout << "colorAttachment.format = " << static_cast<int>(colorAttachment.format) << std::endl;
            std::cout << "depthAttachment.format = " << static_cast<int>(depthAttachment.format) << std::endl;

            mRenderPassHandle = m_rhi->createRenderPass(renderPassDesc);
        }

        void createPipelineLayout() {
            RHI::PipelineLayoutDesc layoutDesc;
            layoutDesc.descriptorSetLayouts = {};
            layoutDesc.pushConstants = {};
			layoutDesc.debugName = "MainPipelineLayout";
            mPipelineLayoutHandle = m_rhi->createPipelineLayout(layoutDesc);
		}

        void createPipeline() {
            RHI::GraphicsPipelineDesc desc;

            RHI::VertexInputState vertexInputState;
            RHI::VertexBinding vertexBinding;
            vertexBinding.binding = 0;
            vertexBinding.stride = RHI::RHIUtils::getFormatSize(RHI::Format::RGB32_Float);
            vertexBinding.inputRate = RHI::VertexInputRate::PerVertex;
            vertexInputState.bindings.push_back(vertexBinding);

            RHI::VertexAttribute vertexAttribute;
            vertexAttribute.location = 0;
            vertexAttribute.binding = 0;
            vertexAttribute.format = RHI::Format::RGB32_Float;
            vertexAttribute.offset = 0;
            vertexAttribute.debugName = "POSITION";
            vertexInputState.attributes.push_back(vertexAttribute);
            //desc.vertexInput = vertexInputState;

            desc.vertexShader =shaderHandles[0];
            desc.fragmentShader = shaderHandles[1];

            desc.topology = RHI::PrimitiveTopology::TriangleList;
            desc.primitiveRestartEnable = false;

            desc.dynamicStates = { RHI::DynamicState::Viewport,RHI::DynamicState::Scissor };
            desc.viewport.viewports = { {0.0f, 0.0f,static_cast<float>(m_width),static_cast<float>(m_height), 0.0f, 1.0f } };
            desc.viewport.scissors = { {{0, 0},{m_width, m_height}} };

            RHI::RasterizerState rasterizerState;
            rasterizerState.cullMode = RHI::CullMode::None;
            rasterizerState.frontFace = RHI::FrontFace::CounterClockwise;
            rasterizerState.lineWidth = 1.0f;
            desc.rasterizer = rasterizerState;

			RHI::MultisampleState multisampleState;
			multisampleState.rasterizationSamples = 1;
			multisampleState.sampleShadingEnable = false;
			desc.multisample = multisampleState;

            RHI::DepthStencilState depthStencilState;
            depthStencilState.depthTestEnable = true;
            depthStencilState.depthWriteEnable = true;
            depthStencilState.depthCompareOp = RHI::CompareOp::Less;
            desc.depthStencil = depthStencilState;

            RHI::ColorBlendState colorBlendState;
            RHI::BlendAttachmentState attachment;
            attachment.blendEnable = false;
            colorBlendState.attachments = { attachment };

            colorBlendState.logicOpEnable = false;  
            colorBlendState.logicOp = RHI::LogicOp::Copy; 
            colorBlendState.blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f };
			desc.colorBlend = colorBlendState;

			desc.pipelineLayoutHandle = mPipelineLayoutHandle;

            desc.renderPass = mRenderPassHandle;
            desc.subpass = 0;
			desc.debugName = "MainPipeline";
            
            mPipelineHandle = m_rhi->createGraphicsPipeline(desc);
        }

        void createFramebuffers() {
			RHI::TextureDesc depthTextureDesc;
			depthTextureDesc.extent = { m_width, m_height, 1 };
			depthTextureDesc.format = m_depthFormat;
            depthTextureDesc.type = RHI::TextureType::Texture2D;
			depthTextureDesc.allowDepthStencil = true;

			mDepthTextureHandle = m_rhi->createDepthTexture(depthTextureDesc);


            mFramebuffers = m_rhi->createFramebuffers(mRenderPassHandle,mDepthTextureHandle);
        }


    private:
        // 窗口相关
        uint32_t m_width = 800*1.25;
        uint32_t m_height = 600*1.25;
        const char* m_title = "StarryEngine";
        const char* m_icon_path = "assets/icons/window_icon.png";
        Window::Ptr m_window;
        bool mFramebufferResized = false;
		uint32_t m_FlightFrame = 2;
        double m_lastFpsTime = 0.0;

        std::shared_ptr<VulkanRHI> m_rhi;

        std::vector<RHI::ShaderHandle> shaderHandles;

		StarryEngine::RHI::BufferHandle mVertexBufferHandle = RHI::BufferHandle::Null();
		StarryEngine::RHI::RenderPassHandle mRenderPassHandle = RHI::RenderPassHandle::Null();
		StarryEngine::RHI::RHIBuffer* mVertexBuffer = nullptr;

		StarryEngine::RHI::PipelineLayoutHandle mPipelineLayoutHandle = RHI::PipelineLayoutHandle::Null();
		StarryEngine::RHI::PipelineHandle mPipelineHandle = RHI::PipelineHandle::Null();

		StarryEngine::RHI::TextureHandle mDepthTextureHandle = RHI::TextureHandle::Null();
        RHI::Format m_depthFormat;

		std::vector<RHI::FramebufferHandle> mFramebuffers;


    };

} // namespace StarryEngine