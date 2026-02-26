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
    constexpr uint32_t ATTACHMENT_UNUSED = std::numeric_limits<uint32_t>::max();

    // 示例：包含位置和颜色
    std::vector<float> vertices = {
        // 背面 (z = -0.5) - 逆时针
        -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,  0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,  0.0f, 1.0f,

        // 正面 (z = 0.5) - 逆时针
        -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.5f, 0.5f, 1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 0.5f, 0.5f,  0.0f, 1.0f,

        // 左面 (x = -0.5) - 逆时针
        -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,  0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,  1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 0.5f, 0.5f,  0.0f, 1.0f,

        // 右面 (x = 0.5) - 逆时针
         0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.5f, 0.5f, 1.0f,  1.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,  0.0f, 1.0f,

         // 顶面 (y = 0.5) - 逆时针
         -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f,
          0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,  1.0f, 0.0f,
          0.5f,  0.5f,  0.5f,  0.5f, 0.5f, 1.0f,  1.0f, 1.0f,
         -0.5f,  0.5f,  0.5f,  1.0f, 0.5f, 0.5f,  0.0f, 1.0f,

         // 底面 (y = -0.5) - 逆时针
         -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,  0.0f, 0.0f,
          0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f,  1.0f, 0.0f,
          0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,  1.0f, 1.0f,
         -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,  0.0f, 1.0f
    };

    std::vector<uint32_t> indices = {
        // 背面
        0, 1, 2,  2, 3, 0,
        // 正面
        4, 5, 6,  6, 7, 4,
        // 左面
        8, 9, 10, 10, 11, 8,
        // 右面
        12, 13, 14, 14, 15, 12,
        // 顶面
        16, 17, 18, 18, 19, 16,
        // 底面
        20, 21, 22, 22, 23, 20
    };

    struct Uniforms {
        glm::mat4 model;   // 偏移 0   （16字节对齐，大小 64字节）
        glm::mat4 view;    // 偏移 64
        glm::mat4 proj;    // 偏移 128
    };

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

            vertexshaderDesc.includePaths = {};
            vertexshaderDesc.debugName = "vertexShader";
            shaderHandles.push_back(m_rhi->createShaderHandle(vertexshaderDesc));

            RHI::ShaderModuleDesc fragmentshaderDesc;
            fragmentshaderDesc.stage = RHI::ShaderStage::Fragment;
            fragmentshaderDesc.sourcecode = R"(
                #version 450
                layout(location = 0) in vec3 fragColor;
                layout(location = 1) in vec2 fragTexCoord;

                layout(location = 0) out vec4 outColor;

                layout(binding = 1) uniform sampler2D texSampler;

                void main() {
                    outColor = texture(texSampler, fragTexCoord) * vec4(fragColor, 1.0);
                    //outColor = vec4(fragTexCoord, 0.0, 1.0);
                }
            )";
            fragmentshaderDesc.includePaths = {};
            fragmentshaderDesc.debugName = "fragmentShader";
            shaderHandles.push_back(m_rhi->createShaderHandle(fragmentshaderDesc));

        }

        void createBuffer() {
            // 创建顶点缓冲区描述
            RHI::BufferDesc bufferDesc;
            bufferDesc.size = vertices.size() * sizeof(float);
            bufferDesc.stride = sizeof(float) * 8; // 位置3 + 颜色3 + 纹理坐标2
            bufferDesc.type = RHI::BufferType::Vertex;
            bufferDesc.memoryType = RHI::MemoryType::CPU_To_GPU;
            bufferDesc.allowUpdate = true;
            bufferDesc.debugName = "MyVertexBuffer";

            mVertexBufferHandle = m_rhi->createBuffer(bufferDesc);
            if (!mVertexBufferHandle.isValid()) {
                std::cerr << "Failed to create vertex buffer!" << std::endl;
                return;
            }
            //更新顶点缓冲区数据
            mVertexBuffer = m_rhi->getBuffer(mVertexBufferHandle);
            mVertexBuffer->update(vertices.data(), vertices.size() * sizeof(float));


            // 创建索引缓冲区
            RHI::BufferDesc indexBufferDesc;
            indexBufferDesc.size = indices.size() * sizeof(uint32_t);
            indexBufferDesc.stride = sizeof(uint32_t);  // 索引数据类型大小
            indexBufferDesc.type = RHI::BufferType::Index;
            indexBufferDesc.memoryType = RHI::MemoryType::CPU_To_GPU;
            indexBufferDesc.allowUpdate = true;
            indexBufferDesc.debugName = "MyIndexBuffer";

            mIndexBufferHandle = m_rhi->createBuffer(indexBufferDesc);
            if (!mIndexBufferHandle.isValid()) {
                std::cerr << "Failed to create index buffer!" << std::endl;
                return;
            }
            //更新索引缓冲区数据
            mIndexBuffer = m_rhi->getBuffer(mIndexBufferHandle);
            mIndexBuffer->update(indices.data(), indices.size() * sizeof(uint32_t));
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
            layoutDesc.descriptorSetLayouts = {mDescriptorSetLayoutHandle};
            layoutDesc.pushConstants = {};
			layoutDesc.debugName = "MainPipelineLayout";
            mPipelineLayoutHandle = m_rhi->createPipelineLayout(layoutDesc);
		}

        void createPipeline() {
            RHI::GraphicsPipelineDesc desc;

            RHI::VertexInputState vertexInputState;
            RHI::VertexBinding vertexBinding;
            vertexBinding.binding = 0;
            vertexBinding.stride = sizeof(float) * 8;  // 位置3 + 颜色3
            vertexBinding.inputRate = RHI::VertexInputRate::PerVertex;
            vertexInputState.bindings.push_back(vertexBinding);

            // 位置属性 (Location 0)
            RHI::VertexAttribute positionAttr;
            positionAttr.location = 0;
            positionAttr.binding = 0;
            positionAttr.format = RHI::Format::RGB32_Float; 
            positionAttr.offset = 0;
            vertexInputState.attributes.push_back(positionAttr);

            // 颜色属性 (Location 1)
            RHI::VertexAttribute colorAttr;
            colorAttr.location = 1;
            colorAttr.binding = 0;
            colorAttr.format = RHI::Format::RGB32_Float;     
            colorAttr.offset = 3 * sizeof(float);
            vertexInputState.attributes.push_back(colorAttr);

            // 纹理坐标属性 (Location 2)
            RHI::VertexAttribute texCoordAttr;
            texCoordAttr.location = 2;
            texCoordAttr.binding = 0;
            texCoordAttr.format = RHI::Format::RG32_Float;   
            texCoordAttr.offset = 6 * sizeof(float);
            vertexInputState.attributes.push_back(texCoordAttr);

            desc.vertexInput = vertexInputState;

            desc.vertexShader =shaderHandles[0];
            desc.fragmentShader = shaderHandles[1];

            desc.topology = RHI::PrimitiveTopology::TriangleList;
            desc.primitiveRestartEnable = false;

            desc.dynamicStates = { RHI::DynamicState::Viewport,RHI::DynamicState::Scissor };
			//为了避免验证层报错，暂时设置一个全屏的默认视口和剪刀矩形，实际渲染时会被动态设置覆盖
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
			depthTextureDesc.debugName = "MainDepthTexture";

			mDepthTextureHandle = m_rhi->createDepthTexture(depthTextureDesc);
            mFramebuffers = m_rhi->createFramebuffers(mRenderPassHandle,mDepthTextureHandle);
        }

        // 第一阶段：创建 uniform buffer、描述符集布局和描述符池
        void createUniformResources() {
            // 1. 创建 Uniform Buffer
            RHI::BufferDesc ubDesc;
            ubDesc.size = sizeof(Uniforms);
            ubDesc.type = RHI::BufferType::Uniform;
            ubDesc.memoryType = RHI::MemoryType::CPU_To_GPU;
            ubDesc.allowUpdate = true;
            ubDesc.persistentMapped = true;
            ubDesc.debugName = "UniformBuffer";
            mUniformBufferHandle = m_rhi->createBuffer(ubDesc);
            mUniformBuffer = m_rhi->getBuffer(mUniformBufferHandle);
            if (!mUniformBuffer) {
                std::cerr << "Failed to create uniform buffer!" << std::endl;
                return;
            }

            // 2. 创建描述符集布局
            RHI::DescriptorSetLayoutDesc layoutDesc;
            // Binding 0: Uniform Buffer
            RHI::DescriptorSetLayoutBinding bindingUBO;
            bindingUBO.binding = 0;
            bindingUBO.type = RHI::DescriptorType::UniformBuffer;
            bindingUBO.count = 1;
            bindingUBO.stageFlags = RHI::ShaderStage::Vertex;
            bindingUBO.immutableSamplers = false;
            layoutDesc.bindings.push_back(bindingUBO);

            // Binding 1: Combined Image Sampler
            RHI::DescriptorSetLayoutBinding bindingTexture;
            bindingTexture.binding = 1;
            bindingTexture.type = RHI::DescriptorType::CombinedImageSampler;
            bindingTexture.count = 1;
            bindingTexture.stageFlags = RHI::ShaderStage::Fragment;
            bindingTexture.immutableSamplers = false;
            layoutDesc.bindings.push_back(bindingTexture);

            mDescriptorSetLayoutHandle = m_rhi->createDescriptorSetLayout(layoutDesc);
            if (!mDescriptorSetLayoutHandle.isValid()) {
                std::cerr << "Failed to create descriptor set layout!" << std::endl;
                return;
            }

            // 3. 创建描述符池
            RHI::DescriptorPoolDesc poolDesc;
            poolDesc.maxSets = 1;
            poolDesc.poolSizes.push_back({ RHI::DescriptorType::UniformBuffer, 1 });
            poolDesc.poolSizes.push_back({ RHI::DescriptorType::CombinedImageSampler, 1 });
            poolDesc.debugName = "MainDescriptorPool";
            mDescriptorPoolHandle = m_rhi->createDescriptorPool(poolDesc);
            if (!mDescriptorPoolHandle.isValid()) {
                std::cerr << "Failed to create descriptor pool!" << std::endl;
                return;
            }
        }

        // 第二阶段：分配并更新描述符集（需要在管线布局创建之后）
        void allocateAndUpdateDescriptorSet() {
            // 4. 分配描述符集
            RHI::DescriptorSetDesc setDesc;
            setDesc.descriptorPool = mDescriptorPoolHandle;
            setDesc.pipelineLayout = mPipelineLayoutHandle;
            setDesc.setIndex = 0;
            setDesc.debugName = "MainDescriptorSet";
            mDescriptorSetHandle = m_rhi->allocateDescriptorSet(setDesc);

            // 5. 更新描述符集
            // 更新描述符集：绑定 Uniform Buffer 到 binding 0
            RHI::DescriptorBufferInfo bufferInfo;
            bufferInfo.buffer = mUniformBufferHandle;
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(Uniforms);
            m_rhi->updateDescriptorSet(mDescriptorSetHandle, 0, 0, bufferInfo);

            // 更新描述符集：绑定纹理+采样器到 binding 1
            RHI::DescriptorImageInfo imageInfo;
            imageInfo.texture = mTextureHandle;
            imageInfo.sampler = mSamplerHandle;
            imageInfo.imageLayout = RHI::ImageLayout::ShaderReadOnly;
            m_rhi->updateDescriptorSet(mDescriptorSetHandle, 1, 0, imageInfo);
        }

        void loadTexture(const char* filename);

        void createSampler() {
            RHI::SamplerDesc samplerDesc;
            samplerDesc.magFilter = RHI::SamplerFilter::Linear;
            samplerDesc.minFilter = RHI::SamplerFilter::Linear;
            samplerDesc.addressU = RHI::SamplerAddressMode::Repeat;
            samplerDesc.addressV = RHI::SamplerAddressMode::Repeat;
            samplerDesc.addressW = RHI::SamplerAddressMode::Repeat;
            samplerDesc.maxAnisotropy = 1.0f;
            samplerDesc.debugName = "MySampler";

            mSamplerHandle = m_rhi->createSampler(samplerDesc);
            mSampler = m_rhi->getSampler(mSamplerHandle);
        }

    private:
        // 窗口相关
        Window::Ptr m_window;
        uint32_t m_width = 800;
        uint32_t m_height = 600;
        const char* m_title = "StarryEngine";
        const char* m_icon_path = "assets/icons/window_icon.png";
        bool mFramebufferResized = false;
		uint32_t m_FlightFrame = 2;
        double m_lastFpsTime = 0.0;

        std::shared_ptr<VulkanRHI> m_rhi;

        std::vector<RHI::ShaderHandle> shaderHandles;

		RHI::BufferHandle mVertexBufferHandle = RHI::BufferHandle::Null();
		RHI::BufferHandle mIndexBufferHandle = RHI::BufferHandle::Null();

        RHI::RHIBuffer* mVertexBuffer = nullptr;
		RHI::RHIBuffer* mIndexBuffer = nullptr;

		RHI::RenderPassHandle mRenderPassHandle = RHI::RenderPassHandle::Null();

		RHI::PipelineLayoutHandle mPipelineLayoutHandle = RHI::PipelineLayoutHandle::Null();
		RHI::PipelineHandle mPipelineHandle = RHI::PipelineHandle::Null();

		RHI::TextureHandle mDepthTextureHandle = RHI::TextureHandle::Null();
        RHI::Format m_depthFormat;
		std::vector<RHI::FramebufferHandle> mFramebuffers;

        RHI::BufferHandle mUniformBufferHandle;
        RHI::DescriptorSetLayoutHandle mDescriptorSetLayoutHandle;
        RHI::DescriptorPoolHandle mDescriptorPoolHandle;
        RHI::DescriptorSetHandle mDescriptorSetHandle;
        RHI::RHIBuffer* mUniformBuffer = nullptr;

        RHI::TextureHandle mTextureHandle;
        RHI::SamplerHandle mSamplerHandle;
        RHI::RHITexture* mTexture = nullptr;
        RHI::RHISampler* mSampler = nullptr;
    };

} // namespace StarryEngine