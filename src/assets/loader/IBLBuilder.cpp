#include "IBLBuilder.hpp"
#include "TextureLoader.hpp"
#include "ShaderLoader.hpp"
#include "../../logging/Logger.hpp"
#include <stb_image.h>
#include <stb_image_write.h>

namespace StarryEngine::Assets {

    // ═══════════════════════════════════════════════
    // 内部辅助类与函数
    // ═══════════════════════════════════════════════

    static RHI::RenderPassHandle createSimpleColorRenderPass(
        RHI::ResourceManager* resMgr,
        RHI::Format colorFormat,
        RHI::AttachmentLoadOp loadOp = RHI::AttachmentLoadOp::DontCare,
        RHI::AttachmentStoreOp storeOp = RHI::AttachmentStoreOp::Store,
        RHI::ImageLayout finalLayout = RHI::ImageLayout::ShaderReadOnly)
    {
        RHI::RenderPassDesc rpDesc;
        RHI::AttachmentDesc colorAtt;
        colorAtt.format = colorFormat;
        colorAtt.sampleCount = 1;
        colorAtt.loadOp = loadOp;
        colorAtt.storeOp = storeOp;
        colorAtt.stencilLoadOp = RHI::AttachmentLoadOp::DontCare;
        colorAtt.stencilStoreOp = RHI::AttachmentStoreOp::DontCare;
        colorAtt.initialLayout = RHI::ImageLayout::Undefined;
        colorAtt.finalLayout = finalLayout;
        rpDesc.attachments.push_back(colorAtt);

        RHI::SubpassDesc subpass;
        subpass.colorAttachments.push_back({ 0, RHI::ImageLayout::ColorAttachment });
        subpass.depthStencilAttachment.attachment = 0xFFFFFFFF;
        subpass.depthStencilAttachment.layout = RHI::ImageLayout::Undefined;
        rpDesc.subpasses.push_back(subpass);

        return resMgr->createRenderPass(rpDesc);
    }

    static RHI::PipelineHandle createFullscreenTrianglePipeline(
        RHI::ResourceManager* resMgr,
        RHI::ShaderHandle         vertShader,
        RHI::ShaderHandle         fragShader,
        RHI::PipelineLayoutHandle pipelineLayout,
        RHI::RenderPassHandle     renderPass,
        uint32_t                  subpass = 0,
        const std::vector<RHI::DynamicState>& dynamicStates =
        { RHI::DynamicState::Viewport, RHI::DynamicState::Scissor })
    {
        RHI::GraphicsPipelineDesc gPipeline;
        gPipeline.vertexShader = vertShader;
        gPipeline.fragmentShader = fragShader;
        gPipeline.topology = RHI::PrimitiveTopology::TriangleList;
        gPipeline.primitiveRestartEnable = false;
        gPipeline.viewport.viewports.resize(1);
        gPipeline.viewport.scissors.resize(1);
        gPipeline.dynamicStates = dynamicStates;
        gPipeline.colorBlend.attachments = { RHI::BlendAttachmentState{} };
        gPipeline.colorBlend.logicOpEnable = false;
        gPipeline.rasterizer.polygonMode = RHI::PolygonMode::Fill;
        gPipeline.rasterizer.cullMode = RHI::CullMode::None;
        gPipeline.rasterizer.frontFace = RHI::FrontFace::CounterClockwise;
        gPipeline.rasterizer.lineWidth = 1.0f;
        gPipeline.multisample.rasterizationSamples = 1;
        gPipeline.depthStencil.depthTestEnable = false;
        gPipeline.depthStencil.depthWriteEnable = false;
        gPipeline.depthStencil.stencilTestEnable = false;
        gPipeline.pipelineLayoutHandle = pipelineLayout;
        gPipeline.renderPass = renderPass;
        gPipeline.subpass = subpass;
        return resMgr->createGraphicsPipeline(gPipeline);
    }

    class OneTimeCommandExecutor {
    public:
        OneTimeCommandExecutor(RHI::IRHI* rhi, RHI::ResourceManager* resMgr)
            : m_rhi(rhi), m_resMgr(resMgr), m_submitted(false) {
            VkDevice vkDevice = static_cast<VkDevice>(m_rhi->getDevice());
            VkCommandPoolCreateInfo poolCI{};
            poolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            poolCI.queueFamilyIndex = m_rhi->getGraphicsQueueFamilyIndex();
            vkCreateCommandPool(vkDevice, &poolCI, nullptr, &m_cmdPool);
            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.commandPool = m_cmdPool;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = 1;
            vkAllocateCommandBuffers(vkDevice, &allocInfo, &m_cmdBuf);
            VkCommandBufferBeginInfo beginBI{};
            beginBI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginBI.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(m_cmdBuf, &beginBI);
            m_encoder = m_rhi->getCommandEncoder(m_cmdBuf);
        }
        ~OneTimeCommandExecutor() {
            if (m_encoder && !m_submitted) {
                m_encoder->end();
                VkSubmitInfo submitInfo{};
                submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &m_cmdBuf;
                VkQueue queue = static_cast<VkQueue>(m_rhi->getGraphicsQueue());
                vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
                vkQueueWaitIdle(queue);
                VkDevice vkDevice = static_cast<VkDevice>(m_rhi->getDevice());
                vkFreeCommandBuffers(vkDevice, m_cmdPool, 1, &m_cmdBuf);
                vkDestroyCommandPool(vkDevice, m_cmdPool, nullptr);
                m_submitted = true;
            }
        }
        RHI::RHICommandEncoder* get() const { return m_encoder.get(); }
    private:
        RHI::IRHI* m_rhi;
        RHI::ResourceManager* m_resMgr;
        VkCommandPool m_cmdPool = VK_NULL_HANDLE;
        VkCommandBuffer m_cmdBuf = VK_NULL_HANDLE;
        std::unique_ptr<RHI::RHICommandEncoder> m_encoder;
        bool m_submitted = false;
    };

    // ═══════════════════════════════════════════════
    // 构造 & 析构
    // ═══════════════════════════════════════════════

    IBLBuilder::IBLBuilder(std::shared_ptr<RHI::ResourceManager> resMgr,
        std::shared_ptr<RHI::IRHI> rhi)
        : m_resMgr(std::move(resMgr)), m_rhi(std::move(rhi))
    {
        // ── 加载图形管线共用的全屏三角形 Shader ──
        Assets::ShaderLoader shaderLoader(m_resMgr);
        auto vsInfo = shaderLoader.loadFromFile(
            "assets/shaders/ibl/equirect_to_cubemap.vert", RHI::ShaderStage::Vertex);
        auto fsInfo = shaderLoader.loadFromFile(
            "assets/shaders/ibl/equirect_to_cubemap.frag", RHI::ShaderStage::Fragment);
        if (vsInfo && fsInfo) {
            m_vs = vsInfo->module;
            m_fs = fsInfo->module;
        }
        else {
            LOG_WARN("IBLBuilder: default fullscreen shaders not loaded");
        }

        // ── 创建图形管线共用的 DescriptorSetLayout ──
        RHI::DescriptorSetLayoutDesc layoutDesc;
        layoutDesc.bindings = {
            { 0, RHI::DescriptorType::CombinedImageSampler, 1,
              RHI::ShaderStage::Fragment }
        };
        m_descLayout = m_resMgr->createDescriptorSetLayout(layoutDesc);

        // ── 创建图形管线共用的 PipelineLayout ──
        RHI::PipelineLayoutDesc plDesc;
        plDesc.descriptorSetLayouts = { m_descLayout };
        plDesc.pushConstants = {
            { RHI::ShaderStage::Fragment, 0, 8 }
        };
        m_pipelineLayout = m_resMgr->createPipelineLayout(plDesc);
    }

    IBLBuilder::~IBLBuilder() {
        if (m_pipelineLayout.isValid()) m_resMgr->destroy(m_pipelineLayout);
        if (m_descLayout.isValid())     m_resMgr->destroy(m_descLayout);
        if (m_vs.isValid())             m_resMgr->destroy(m_vs);
        if (m_fs.isValid())             m_resMgr->destroy(m_fs);
    }

    // ═══════════════════════════════════════════════
    // 图形管线：内部辅助
    // ═══════════════════════════════════════════════

    RHI::DescriptorSetHandle IBLBuilder::createDescriptorSet(
        RHI::TextureHandle      texture,
        RHI::SamplerHandle      sampler,
        RHI::DescriptorSetLayoutHandle layout,
        RHI::DescriptorPoolHandle      pool)
    {
        RHI::DescriptorSetDesc setDesc;
        setDesc.descriptorSetLayout = layout;
        setDesc.descriptorPool = pool;
        auto descSet = m_resMgr->createDescriptorSet(setDesc);
        if (!descSet.isValid()) return descSet;

        auto* setObj = m_resMgr->getDescriptorSet(descSet);
        auto* texObj = m_resMgr->getTexture(texture);
        auto* samplerObj = m_resMgr->getSampler(sampler);
        if (setObj && texObj && samplerObj) {
            setObj->writeTexture(0, 0, texObj, samplerObj,
                RHI::ImageLayout::ShaderReadOnly);
            setObj->update();
        }
        return descSet;
    }

    // ═══════════════════════════════════════════════
    // 计算管线：内部辅助
    // ═══════════════════════════════════════════════

    RHI::DescriptorSetHandle IBLBuilder::createComputeDescriptorSet(
        RHI::TextureHandle      inputTex,
        RHI::SamplerHandle      sampler,
        RHI::TextureHandle      outputTex,
        RHI::DescriptorSetLayoutHandle layout,
        RHI::DescriptorPoolHandle      pool,
        RHI::ImageLayout        outputLayout)
    {
        RHI::DescriptorSetDesc setDesc;
        setDesc.descriptorSetLayout = layout;
        setDesc.descriptorPool = pool;
        auto descSet = m_resMgr->createDescriptorSet(setDesc);
        if (!descSet.isValid()) return descSet;

        auto* setObj = m_resMgr->getDescriptorSet(descSet);
        setObj->writeTexture(0, 0,
            m_resMgr->getTexture(inputTex),
            m_resMgr->getSampler(sampler),
            RHI::ImageLayout::ShaderReadOnly);
        setObj->writeTexture(1, 0,
            m_resMgr->getTexture(outputTex),
            nullptr,
            outputLayout);
        return descSet;
    }

    // ═══════════════════════════════════════════════
    // 图形管线：equirect → cubemap
    // ═══════════════════════════════════════════════

    RHI::TextureHandle IBLBuilder::equirectToCubemap(
        RHI::TextureHandle equirectTex,
        uint32_t            faceSize)
    {
        // ── 计算 mip 层数 ──
        uint32_t mipLevels = 1;
        {
            uint32_t s = faceSize;
            while (s > 1) { s >>= 1; ++mipLevels; }
        }

        RHI::TextureDesc cubemapDesc;
        cubemapDesc.extent = { faceSize, faceSize, 1 };
        cubemapDesc.format = RHI::Format::RGBA32_Float;
        cubemapDesc.type = RHI::TextureType::TextureCube;
        cubemapDesc.mipLevels = mipLevels;
        cubemapDesc.arrayLayers = 6;
        cubemapDesc.sampleCount = 1;
        cubemapDesc.flags = RHI::ImageCreateFlags::CubeCompatible;
        cubemapDesc.allowRenderTarget = true;
        cubemapDesc.debugName = "EnvCubemap";
        auto cubemap = m_resMgr->createTexture(cubemapDesc);
        if (!cubemap.isValid()) return cubemap;

        RHI::SamplerDesc sampDesc;
        sampDesc.minFilter = RHI::SamplerFilter::Linear;
        sampDesc.magFilter = RHI::SamplerFilter::Linear;
        sampDesc.addressU = RHI::SamplerAddressMode::ClampToEdge;
        sampDesc.addressV = RHI::SamplerAddressMode::ClampToEdge;
        sampDesc.addressW = RHI::SamplerAddressMode::ClampToEdge;
        sampDesc.maxLod = 1.0f;
        auto sampler = m_resMgr->createSampler(sampDesc);

        RHI::DescriptorPoolDesc poolDesc;
        poolDesc.maxSets = 1;
        poolDesc.poolSizes = { { RHI::DescriptorType::CombinedImageSampler, 1 } };
        poolDesc.freeDescriptorSet = true;
        auto pool = m_resMgr->createDescriptorPool(poolDesc);
        auto descSet = createDescriptorSet(equirectTex, sampler, m_descLayout, pool);

        auto renderPass = createSimpleColorRenderPass(
            m_resMgr.get(), RHI::Format::RGBA32_Float,
            RHI::AttachmentLoadOp::DontCare, RHI::AttachmentStoreOp::Store,
            RHI::ImageLayout::ShaderReadOnly);
        auto pipeline = createFullscreenTrianglePipeline(
            m_resMgr.get(), m_vs, m_fs, m_pipelineLayout, renderPass);

        std::vector<RHI::FramebufferHandle> framebuffers(6);
        for (int face = 0; face < 6; ++face) {
            RHI::ImageSubresourceRange range;
            range.aspectMask = RHI::ImageAspect::Color;
            range.baseMipLevel = 0;
            range.levelCount = 1;
            range.baseArrayLayer = static_cast<uint32_t>(face);
            range.layerCount = 1;
            void* viewKey = m_resMgr->getTexture(cubemap)->createView(range, RHI::ImageViewType::Texture2D);
            void* nativeView = m_resMgr->getTexture(cubemap)->getNativeHandleFromView(viewKey);

            RHI::FramebufferDesc fbDesc;
            fbDesc.renderPass = m_resMgr->getRenderPass(renderPass)->getNativeHandle();
            fbDesc.extent = { faceSize, faceSize };
            fbDesc.attachments = { nativeView };
            fbDesc.layers = 1;
            framebuffers[face] = m_resMgr->createFramebuffer(fbDesc);
        }

        {
            OneTimeCommandExecutor executor(m_rhi.get(), m_resMgr.get());
            auto* cmd = executor.get();
            auto* pipelineObj = m_resMgr->getPipeline(pipeline);
            auto* pipelineLayoutObj = m_resMgr->getPipelineLayout(m_pipelineLayout);

            for (int face = 0; face < 6; ++face) {
                RHI::RenderPassBeginInfo rpBegin;
                rpBegin.renderPass = m_resMgr->getRenderPass(renderPass)->getNativeHandle();
                rpBegin.framebuffer = m_resMgr->getFramebuffer(framebuffers[face])->getNativeHandle();
                rpBegin.renderArea = { {0, 0}, {faceSize, faceSize} };
                rpBegin.clearValues = { {0.0f, 0.0f, 0.0f, 0.0f} };
                cmd->beginRenderPass(rpBegin, RHI::SubpassContents::Inline);
                cmd->bindPipeline(pipelineObj);
                cmd->bindDescriptorSets(RHI::PipelineBindPoint::Graphics, pipelineLayoutObj,
                    0, { descSet }, {});
                struct { int face; float faceSize; } pc = { face, float(faceSize) };
                cmd->pushConstants(pipelineLayoutObj, RHI::ShaderStage::Fragment,
                    0, sizeof(pc), &pc);
                cmd->setViewport({ 0.0f, 0.0f, float(faceSize), float(faceSize), 0.0f, 1.0f });
                cmd->setScissor({ {0, 0}, {faceSize, faceSize} });
                cmd->draw(3, 1, 0, 0);
                cmd->endRenderPass();
            }

        }

        // ═══════════════════════════════════════════════
        // 生成源 Cubemap 的 mip 链（图形管线路径）
        //   渲染完成后 mip 0 处于 ShaderReadOnly，
        //   用 cubemap_downsample.comp 在 3D 方向空间降采样
        // ═══════════════════════════════════════════════
        if (mipLevels > 1) {
            Assets::ShaderLoader dsLoader(m_resMgr);
            auto dsInfo = dsLoader.loadFromFile(
                "assets/shaders/ibl/cubemap_downsample.comp", RHI::ShaderStage::Compute);
            if (dsInfo) {
                auto dsShader = dsInfo->module;
                auto* cubemapObj = m_resMgr->getTexture(cubemap);

                RHI::DescriptorSetLayoutDesc dsLayoutDesc;
                dsLayoutDesc.bindings = {
                    { 0, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Compute },
                    { 1, RHI::DescriptorType::StorageImage,         1, RHI::ShaderStage::Compute }
                };
                auto dsDescLayout = m_resMgr->createDescriptorSetLayout(dsLayoutDesc);

                RHI::PipelineLayoutDesc dsPlDesc;
                dsPlDesc.descriptorSetLayouts = { dsDescLayout };
                dsPlDesc.pushConstants = { { RHI::ShaderStage::Compute, 0, 8 } };
                auto dsPlLayout = m_resMgr->createPipelineLayout(dsPlDesc);

                RHI::ComputePipelineDesc dsCompDesc;
                dsCompDesc.computeShader = dsShader;
                dsCompDesc.pipelineLayoutHandle = dsPlLayout;
                auto dsPipeline = m_resMgr->createComputePipeline(dsCompDesc);

                RHI::SamplerDesc dsSampDesc;
                dsSampDesc.minFilter = RHI::SamplerFilter::Linear;
                dsSampDesc.magFilter = RHI::SamplerFilter::Linear;
                dsSampDesc.addressU = RHI::SamplerAddressMode::ClampToEdge;
                dsSampDesc.addressV = RHI::SamplerAddressMode::ClampToEdge;
                dsSampDesc.addressW = RHI::SamplerAddressMode::ClampToEdge;
                auto dsSampler = m_resMgr->createSampler(dsSampDesc);
                auto* dsSamplerObj = m_resMgr->getSampler(dsSampler);

                RHI::DescriptorPoolDesc dsPoolDesc;
                dsPoolDesc.maxSets = 64;
                dsPoolDesc.poolSizes = {
                    { RHI::DescriptorType::CombinedImageSampler, 64 },
                    { RHI::DescriptorType::StorageImage,         64 }
                };
                dsPoolDesc.freeDescriptorSet = false;
                auto dsPool = m_resMgr->createDescriptorPool(dsPoolDesc);

                RHI::ImageSubresourceRange srcOnlyMip0;
                srcOnlyMip0.aspectMask = RHI::ImageAspect::Color;
                srcOnlyMip0.baseMipLevel = 0;
                srcOnlyMip0.levelCount = 1;
                srcOnlyMip0.baseArrayLayer = 0;
                srcOnlyMip0.layerCount = 6;
                void* srcMip0View = cubemapObj->createView(srcOnlyMip0, RHI::ImageViewType::TextureCube);
                void* srcMip0Native = cubemapObj->getNativeHandleFromView(srcMip0View);

                for (uint32_t mip = 1; mip < mipLevels; ++mip) {
                    uint32_t srcSize = faceSize >> (mip - 1);
                    uint32_t dstSize = srcSize / 2;
                    uint32_t gx = (dstSize + 15) / 16;
                    uint32_t gy = (dstSize + 15) / 16;

                    // 过渡目标 mip → General（用于 imageStore 写入）
                    RHI::ImageSubresourceRange mipRange;
                    mipRange.aspectMask = RHI::ImageAspect::Color;
                    mipRange.baseMipLevel = mip;
                    mipRange.levelCount = 1;
                    mipRange.baseArrayLayer = 0;
                    mipRange.layerCount = 6;
                    cubemapObj->transitionLayout(
                        RHI::ImageLayout::General,
                        RHI::PipelineStage::TopOfPipe,
                        RHI::PipelineStage::ComputeShader,
                        static_cast<RHI::AccessFlags>(0),
                        static_cast<RHI::AccessFlags>(RHI::AccessFlag::ShaderWrite),
                        mipRange);

                    std::vector<void*> faceViews;
                    std::vector<RHI::DescriptorSetHandle> faceDescSets;
                    for (int f = 0; f < 6; ++f) {
                        RHI::ImageSubresourceRange faceRange;
                        faceRange.aspectMask = RHI::ImageAspect::Color;
                        faceRange.baseMipLevel = mip;
                        faceRange.levelCount = 1;
                        faceRange.baseArrayLayer = uint32_t(f);
                        faceRange.layerCount = 1;
                        void* vk = cubemapObj->createView(faceRange, RHI::ImageViewType::Texture2D);
                        void* nv = cubemapObj->getNativeHandleFromView(vk);
                        faceViews.push_back(vk);

                        RHI::DescriptorSetDesc setDesc;
                        setDesc.descriptorSetLayout = dsDescLayout;
                        setDesc.descriptorPool = dsPool;
                        auto dsSet = m_resMgr->createDescriptorSet(setDesc);
                        auto* setObj = m_resMgr->getDescriptorSet(dsSet);
                        setObj->writeTextureCustomView(0, 0, srcMip0Native, dsSamplerObj,
                            RHI::ImageLayout::ShaderReadOnly);
                        setObj->writeTextureCustomView(1, 0, nv, nullptr,
                            RHI::ImageLayout::General);
                        setObj->update();
                        faceDescSets.push_back(dsSet);
                    }

                    {
                        OneTimeCommandExecutor dsExec(m_rhi.get(), m_resMgr.get());
                        auto* dsCmd = dsExec.get();
                        dsCmd->bindComputePipeline(m_resMgr->getPipeline(dsPipeline));

                        for (int f = 0; f < 6; ++f) {
                            dsCmd->bindDescriptorSets(RHI::PipelineBindPoint::Compute,
                                m_resMgr->getPipelineLayout(dsPlLayout), 0,
                                { faceDescSets[f] }, {});
                            struct { int face; float srcSize; } pc;
                            pc.face = f; pc.srcSize = float(srcSize);
                            dsCmd->pushConstants(m_resMgr->getPipelineLayout(dsPlLayout),
                                RHI::ShaderStage::Compute, 0, sizeof(pc), &pc);
                            dsCmd->dispatch(gx, gy, 1);
                        }
                    }

                    for (auto& set : faceDescSets) m_resMgr->destroy(set);
                    for (auto* vk : faceViews) cubemapObj->destroyView(vk);

                    cubemapObj->transitionLayout(
                        RHI::ImageLayout::ShaderReadOnly,
                        RHI::PipelineStage::ComputeShader,
                        RHI::PipelineStage::AllCommands,
                        static_cast<RHI::AccessFlags>(RHI::AccessFlag::ShaderWrite),
                        static_cast<RHI::AccessFlags>(RHI::AccessFlag::ShaderRead),
                        mipRange);
                }

                m_resMgr->destroy(dsPool);
                m_resMgr->destroy(dsPipeline);
                m_resMgr->destroy(dsShader);
                m_resMgr->destroy(dsPlLayout);
                m_resMgr->destroy(dsDescLayout);
                m_resMgr->destroy(dsSampler);
                cubemapObj->destroyView(srcMip0View);
            }
        }

        for (auto fb : framebuffers) m_resMgr->destroy(fb);
        m_resMgr->destroy(renderPass);
        m_resMgr->destroy(pipeline);
        m_resMgr->destroy(sampler);
        m_resMgr->destroy(descSet);
        m_resMgr->destroy(pool);
        return cubemap;
    }

    // ═══════════════════════════════════════════════
    // 图形管线：Irradiance Map
    // ═══════════════════════════════════════════════

    RHI::TextureHandle IBLBuilder::generateIrradianceMap(
        RHI::TextureHandle envCubemap,
        uint32_t            outputSize)
    {

        if (!envCubemap.isValid()) return RHI::TextureHandle::Null();

        Assets::ShaderLoader loader(m_resMgr);
        auto vsInfo = loader.loadFromFile("assets/shaders/ibl/irradiance_convolution.vert", RHI::ShaderStage::Vertex);
        auto fsInfo = loader.loadFromFile("assets/shaders/ibl/irradiance_convolution.frag", RHI::ShaderStage::Fragment);
        if (!vsInfo || !fsInfo) return RHI::TextureHandle::Null();
        RHI::ShaderHandle irradVS = vsInfo->module, irradFS = fsInfo->module;

        RHI::DescriptorSetLayoutDesc layoutDesc;
        layoutDesc.bindings = { {0, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Fragment} };
        auto descLayout = m_resMgr->createDescriptorSetLayout(layoutDesc);

        RHI::PipelineLayoutDesc plDesc;
        plDesc.descriptorSetLayouts = { descLayout };
        plDesc.pushConstants = { {RHI::ShaderStage::Fragment, 0, 8} };
        auto plLayout = m_resMgr->createPipelineLayout(plDesc);

        RHI::DescriptorPoolDesc poolDesc;
        poolDesc.maxSets = 1;
        poolDesc.poolSizes = { {RHI::DescriptorType::CombinedImageSampler, 1} };
        poolDesc.freeDescriptorSet = true;
        auto pool = m_resMgr->createDescriptorPool(poolDesc);

        RHI::SamplerDesc sampDesc;
        sampDesc.minFilter = RHI::SamplerFilter::Linear;
        sampDesc.magFilter = RHI::SamplerFilter::Linear;
        sampDesc.addressU = RHI::SamplerAddressMode::ClampToEdge;
        sampDesc.addressV = RHI::SamplerAddressMode::ClampToEdge;
        sampDesc.addressW = RHI::SamplerAddressMode::ClampToEdge;
        auto sampler = m_resMgr->createSampler(sampDesc);
        auto descSet = createDescriptorSet(envCubemap, sampler, descLayout, pool);

        RHI::TextureDesc irradDesc;
        irradDesc.extent = { outputSize, outputSize, 1 };
        irradDesc.format = RHI::Format::RGBA32_Float;
        irradDesc.type = RHI::TextureType::TextureCube;
        irradDesc.mipLevels = 1;
        irradDesc.arrayLayers = 6;
        irradDesc.sampleCount = 1;
        irradDesc.flags = RHI::ImageCreateFlags::CubeCompatible;
        irradDesc.allowRenderTarget = true;
        irradDesc.debugName = "IrradianceMap";
        auto irradMap = m_resMgr->createTexture(irradDesc);
        if (!irradMap.isValid()) return irradMap;

        auto renderPass = createSimpleColorRenderPass(
            m_resMgr.get(), RHI::Format::RGBA32_Float);
        auto pipeline = createFullscreenTrianglePipeline(
            m_resMgr.get(), irradVS, irradFS, plLayout, renderPass);

        std::vector<RHI::FramebufferHandle> framebuffers(6);
        for (int face = 0; face < 6; ++face) {
            RHI::ImageSubresourceRange range;
            range.aspectMask = RHI::ImageAspect::Color;
            range.baseMipLevel = 0;
            range.levelCount = 1;
            range.baseArrayLayer = static_cast<uint32_t>(face);
            range.layerCount = 1;
            void* vk = m_resMgr->getTexture(irradMap)->createView(range, RHI::ImageViewType::Texture2D);
            void* nv = m_resMgr->getTexture(irradMap)->getNativeHandleFromView(vk);
            RHI::FramebufferDesc fbDesc;
            fbDesc.renderPass = m_resMgr->getRenderPass(renderPass)->getNativeHandle();
            fbDesc.extent = { outputSize, outputSize };
            fbDesc.attachments = { nv };
            fbDesc.layers = 1;
            framebuffers[face] = m_resMgr->createFramebuffer(fbDesc);
        }

        {
            OneTimeCommandExecutor executor(m_rhi.get(), m_resMgr.get());
            auto* cmd = executor.get();
            auto* ppl = m_resMgr->getPipeline(pipeline);
            auto* plo = m_resMgr->getPipelineLayout(plLayout);
            for (int face = 0; face < 6; ++face) {
                RHI::RenderPassBeginInfo rpBegin;
                rpBegin.renderPass = m_resMgr->getRenderPass(renderPass)->getNativeHandle();
                rpBegin.framebuffer = m_resMgr->getFramebuffer(framebuffers[face])->getNativeHandle();
                rpBegin.renderArea = { {0, 0}, {outputSize, outputSize} };
                rpBegin.clearValues = { {0.0f, 0.0f, 0.0f, 0.0f} };
                cmd->beginRenderPass(rpBegin, RHI::SubpassContents::Inline);
                cmd->bindPipeline(ppl);
                cmd->bindDescriptorSets(RHI::PipelineBindPoint::Graphics, plo, 0, { descSet }, {});
                struct { int face; float faceSize; } pc = { face, float(outputSize) };
                cmd->pushConstants(plo, RHI::ShaderStage::Fragment, 0, sizeof(pc), &pc);
                cmd->setViewport({ 0.0f, 0.0f, float(outputSize), float(outputSize), 0.0f, 1.0f });
                cmd->setScissor({ {0, 0}, {outputSize, outputSize} });
                cmd->draw(3, 1, 0, 0);
                cmd->endRenderPass();
            }
        }

        for (auto fb : framebuffers) m_resMgr->destroy(fb);
        m_resMgr->destroy(renderPass);
        m_resMgr->destroy(pipeline);
        m_resMgr->destroy(plLayout);
        m_resMgr->destroy(descLayout);
        m_resMgr->destroy(pool);
        m_resMgr->destroy(sampler);
        m_resMgr->destroy(descSet);
        m_resMgr->destroy(irradVS);
        m_resMgr->destroy(irradFS);
        LOG_INFO("Irradiance map (graphics) generated: {}x{}", outputSize, outputSize);
        return irradMap;
    }

    // ═══════════════════════════════════════════════
    // 图形管线：Prefiltered Map
    // ═══════════════════════════════════════════════

    RHI::TextureHandle IBLBuilder::generatePrefilteredMap(
        RHI::TextureHandle envCubemap,
        uint32_t            baseSize,
        uint32_t            mipLevels)
    {
        if (!envCubemap.isValid()) return RHI::TextureHandle::Null();

        Assets::ShaderLoader loader(m_resMgr);
        auto vsInfo = loader.loadFromFile("assets/shaders/ibl/prefilter_envmap.vert", RHI::ShaderStage::Vertex);
        auto fsInfo = loader.loadFromFile("assets/shaders/ibl/prefilter_envmap.frag", RHI::ShaderStage::Fragment);
        if (!vsInfo || !fsInfo) return RHI::TextureHandle::Null();
        RHI::ShaderHandle vs = vsInfo->module, fs = fsInfo->module;

        RHI::DescriptorSetLayoutDesc layoutDesc;
        layoutDesc.bindings = { {0, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Fragment} };
        auto descLayout = m_resMgr->createDescriptorSetLayout(layoutDesc);

        RHI::PipelineLayoutDesc plDesc;
        plDesc.descriptorSetLayouts = { descLayout };
        plDesc.pushConstants = { {RHI::ShaderStage::Fragment, 0, 16} };
        auto plLayout = m_resMgr->createPipelineLayout(plDesc);

        RHI::DescriptorPoolDesc poolDesc;
        poolDesc.maxSets = 1;
        poolDesc.poolSizes = { {RHI::DescriptorType::CombinedImageSampler, 1} };
        poolDesc.freeDescriptorSet = true;
        auto pool = m_resMgr->createDescriptorPool(poolDesc);

        RHI::SamplerDesc sampDesc;
        sampDesc.minFilter = RHI::SamplerFilter::Linear;
        sampDesc.magFilter = RHI::SamplerFilter::Linear;
        sampDesc.addressU = RHI::SamplerAddressMode::ClampToEdge;
        sampDesc.addressV = RHI::SamplerAddressMode::ClampToEdge;
        sampDesc.addressW = RHI::SamplerAddressMode::ClampToEdge;
        sampDesc.maxLod = 32.0f;   // 允许 access 源 cubemap 的全部 mip
        auto sampler = m_resMgr->createSampler(sampDesc);
        auto descSet = createDescriptorSet(envCubemap, sampler, descLayout, pool);

        RHI::TextureDesc prefDesc;
        prefDesc.extent = { baseSize, baseSize, 1 };
        prefDesc.format = RHI::Format::RGBA16_Float;
        prefDesc.type = RHI::TextureType::TextureCube;
        prefDesc.mipLevels = mipLevels;
        prefDesc.arrayLayers = 6;
        prefDesc.sampleCount = 1;
        prefDesc.flags = RHI::ImageCreateFlags::CubeCompatible;
        prefDesc.allowRenderTarget = true;
        prefDesc.debugName = "PrefilteredEnvMap";
        auto prefilteredMap = m_resMgr->createTexture(prefDesc);
        if (!prefilteredMap.isValid()) return prefilteredMap;

        auto renderPass = createSimpleColorRenderPass(
            m_resMgr.get(), RHI::Format::RGBA16_Float);
        auto pipeline = createFullscreenTrianglePipeline(
            m_resMgr.get(), vs, fs, plLayout, renderPass);

        std::vector<RHI::FramebufferHandle> tempFBs;
        std::vector<void*> tempViews;

        {
            OneTimeCommandExecutor executor(m_rhi.get(), m_resMgr.get());
            auto* cmd = executor.get();
            auto* ppl = m_resMgr->getPipeline(pipeline);
            auto* plo = m_resMgr->getPipelineLayout(plLayout);

            float sourceFaceSize = static_cast<float>(m_resMgr->getTexture(envCubemap)->getExtent().width);
            for (uint32_t mip = 0; mip < mipLevels; ++mip) {
                uint32_t mipSize = baseSize >> mip;
                float roughness = float(mip) / float(mipLevels - 1);
                for (int face = 0; face < 6; ++face) {
                    RHI::ImageSubresourceRange range;
                    range.aspectMask = RHI::ImageAspect::Color;
                    range.baseMipLevel = mip;
                    range.levelCount = 1;
                    range.baseArrayLayer = static_cast<uint32_t>(face);
                    range.layerCount = 1;
                    void* vk = m_resMgr->getTexture(prefilteredMap)->createView(range, RHI::ImageViewType::Texture2D);
                    void* nv = m_resMgr->getTexture(prefilteredMap)->getNativeHandleFromView(vk);
                    tempViews.push_back(vk);

                    RHI::FramebufferDesc fbDesc;
                    fbDesc.renderPass = m_resMgr->getRenderPass(renderPass)->getNativeHandle();
                    fbDesc.extent = { mipSize, mipSize };
                    fbDesc.attachments = { nv };
                    fbDesc.layers = 1;
                    auto fb = m_resMgr->createFramebuffer(fbDesc);
                    tempFBs.push_back(fb);

                    RHI::RenderPassBeginInfo rpBegin;
                    rpBegin.renderPass = m_resMgr->getRenderPass(renderPass)->getNativeHandle();
                    rpBegin.framebuffer = m_resMgr->getFramebuffer(fb)->getNativeHandle();
                    rpBegin.renderArea = { {0, 0}, {mipSize, mipSize} };
                    rpBegin.clearValues = { {0.0f, 0.0f, 0.0f, 0.0f} };
                    cmd->beginRenderPass(rpBegin, RHI::SubpassContents::Inline);
                    cmd->bindPipeline(ppl);
                    cmd->bindDescriptorSets(RHI::PipelineBindPoint::Graphics, plo, 0, { descSet }, {});
                    struct { int face; float faceSize; float roughness; float envResolution; } pc;
                    pc.face = face; pc.faceSize = float(mipSize); pc.roughness = roughness;
                    pc.envResolution = sourceFaceSize;
                    cmd->pushConstants(plo, RHI::ShaderStage::Fragment, 0, sizeof(pc), &pc);
                    cmd->setViewport({ 0.0f, 0.0f, float(mipSize), float(mipSize), 0.0f, 1.0f });
                    cmd->setScissor({ {0, 0}, {mipSize, mipSize} });
                    cmd->draw(3, 1, 0, 0);
                    cmd->endRenderPass();
                }
            }
        }

        for (auto& fb : tempFBs) m_resMgr->destroy(fb);
        for (auto& vk : tempViews) m_resMgr->getTexture(prefilteredMap)->destroyView(vk);
        m_resMgr->destroy(renderPass);
        m_resMgr->destroy(pipeline);
        m_resMgr->destroy(plLayout);
        m_resMgr->destroy(descLayout);
        m_resMgr->destroy(pool);
        m_resMgr->destroy(sampler);
        m_resMgr->destroy(descSet);
        m_resMgr->destroy(vs);
        m_resMgr->destroy(fs);
        LOG_INFO("Prefiltered map (graphics) generated: {}x{} ({} mips)", baseSize, baseSize, mipLevels);
        return prefilteredMap;
    }

    // ═══════════════════════════════════════════════
    // 图形管线：BRDF LUT
    // ═══════════════════════════════════════════════

    RHI::TextureHandle IBLBuilder::generateBrdfLut(uint32_t size) {
        Assets::ShaderLoader loader(m_resMgr);
        auto vsInfo = loader.loadFromFile("assets/shaders/ibl/brdf_lut.vert", RHI::ShaderStage::Vertex);
        auto fsInfo = loader.loadFromFile("assets/shaders/ibl/brdf_lut.frag", RHI::ShaderStage::Fragment);
        if (!vsInfo || !fsInfo) return RHI::TextureHandle::Null();
        RHI::ShaderHandle vs = vsInfo->module, fs = fsInfo->module;

        RHI::PipelineLayoutDesc plDesc;
        auto plLayout = m_resMgr->createPipelineLayout(plDesc);

        RHI::TextureDesc texDesc;
        texDesc.extent = { size, size, 1 };
        texDesc.format = RHI::Format::RG16_Float;
        texDesc.type = RHI::TextureType::Texture2D;
        texDesc.mipLevels = 1;
        texDesc.arrayLayers = 1;
        texDesc.sampleCount = 1;
        texDesc.allowRenderTarget = true;
        texDesc.debugName = "BrdfLUT";
        auto brdfLut = m_resMgr->createTexture(texDesc);
        if (!brdfLut.isValid()) return brdfLut;

        auto renderPass = createSimpleColorRenderPass(m_resMgr.get(), RHI::Format::RG16_Float);
        auto pipeline = createFullscreenTrianglePipeline(m_resMgr.get(), vs, fs, plLayout, renderPass);

        RHI::ImageSubresourceRange range;
        range.aspectMask = RHI::ImageAspect::Color;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;
        void* vk = m_resMgr->getTexture(brdfLut)->createView(range, RHI::ImageViewType::Texture2D);
        void* nv = m_resMgr->getTexture(brdfLut)->getNativeHandleFromView(vk);
        RHI::FramebufferDesc fbDesc;
        fbDesc.renderPass = m_resMgr->getRenderPass(renderPass)->getNativeHandle();
        fbDesc.extent = { size, size };
        fbDesc.attachments = { nv };
        fbDesc.layers = 1;
        auto framebuffer = m_resMgr->createFramebuffer(fbDesc);

        {
            OneTimeCommandExecutor executor(m_rhi.get(), m_resMgr.get());
            auto* cmd = executor.get();
            RHI::RenderPassBeginInfo rpBegin;
            rpBegin.renderPass = m_resMgr->getRenderPass(renderPass)->getNativeHandle();
            rpBegin.framebuffer = m_resMgr->getFramebuffer(framebuffer)->getNativeHandle();
            rpBegin.renderArea = { {0, 0}, {size, size} };
            rpBegin.clearValues = { {0.0f, 0.0f, 0.0f, 0.0f} };
            cmd->beginRenderPass(rpBegin, RHI::SubpassContents::Inline);
            cmd->bindPipeline(m_resMgr->getPipeline(pipeline));
            cmd->setViewport({ 0.0f, 0.0f, float(size), float(size), 0.0f, 1.0f });
            cmd->setScissor({ {0, 0}, {size, size} });
            cmd->draw(3, 1, 0, 0);
            cmd->endRenderPass();
        }

        m_resMgr->destroy(framebuffer);
        m_resMgr->destroy(renderPass);
        m_resMgr->destroy(pipeline);
        m_resMgr->destroy(plLayout);
        m_resMgr->destroy(vs);
        m_resMgr->destroy(fs);
        LOG_INFO("BRDF LUT (graphics) generated: {}x{}", size, size);
        return brdfLut;
    }

    // ═══════════════════════════════════════════════
    // 计算管线：equirect → cubemap
    // ═══════════════════════════════════════════════

    RHI::TextureHandle IBLBuilder::equirectToCubemapCS(
        RHI::TextureHandle equirectTex,
        uint32_t            faceSize)
    {
        // ── 计算 mip 层数 ──
        uint32_t mipLevels = 1;
        {
            uint32_t s = faceSize;
            while (s > 1) { s >>= 1; ++mipLevels; }
        }

        // ── 创建设备端 Cubemap 纹理（6 层，允许 storage，含完整 mip 链）──
        RHI::TextureDesc cubemapDesc;
        cubemapDesc.extent = { faceSize, faceSize, 1 };
        cubemapDesc.format = RHI::Format::RGBA32_Float;
        cubemapDesc.type = RHI::TextureType::TextureCube;
        cubemapDesc.mipLevels = mipLevels;
        cubemapDesc.arrayLayers = 6;
        cubemapDesc.sampleCount = 1;
        cubemapDesc.flags = RHI::ImageCreateFlags::CubeCompatible;
        cubemapDesc.allowUnorderedAccess = true;
        cubemapDesc.allowRenderTarget = false;
        cubemapDesc.debugName = "EnvCubemap";
        auto cubemap = m_resMgr->createTexture(cubemapDesc);
        if (!cubemap.isValid()) return cubemap;

        // ── 加载 Compute Shader ──
        Assets::ShaderLoader shaderLoader(m_resMgr);
        auto csInfo = shaderLoader.loadFromFile(
            "assets/shaders/ibl/equirect_to_cubemap.comp", RHI::ShaderStage::Compute);
        if (!csInfo) return RHI::TextureHandle::Null();
        auto computeShader = csInfo->module;

        // ── 创建 DescriptorSetLayout ──
        RHI::DescriptorSetLayoutDesc layoutDesc;
        layoutDesc.bindings = {
            { 0, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Compute },
            { 1, RHI::DescriptorType::StorageImage,         1, RHI::ShaderStage::Compute }
        };
        auto descLayout = m_resMgr->createDescriptorSetLayout(layoutDesc);

        // ── 创建 PipelineLayout (push constant 传 faceSize) ──
        RHI::PipelineLayoutDesc plDesc;
        plDesc.descriptorSetLayouts = { descLayout };
        plDesc.pushConstants = { { RHI::ShaderStage::Compute, 0, sizeof(uint32_t) } };
        auto plLayout = m_resMgr->createPipelineLayout(plDesc);

        // ── 创建 ComputePipeline ──
        RHI::ComputePipelineDesc compDesc;
        compDesc.computeShader = computeShader;
        compDesc.pipelineLayoutHandle = plLayout;
        auto pipeline = m_resMgr->createComputePipeline(compDesc);

        // ── 创建采样器 ──
        RHI::SamplerDesc sampDesc;
        sampDesc.minFilter = RHI::SamplerFilter::Linear;
        sampDesc.magFilter = RHI::SamplerFilter::Linear;
        sampDesc.addressU = RHI::SamplerAddressMode::ClampToEdge;
        sampDesc.addressV = RHI::SamplerAddressMode::ClampToEdge;
        auto sampler = m_resMgr->createSampler(sampDesc);

        // ── ★ 创建覆盖全部 6 个面的 Cube 视图（compute shader 只写 mip 0）──
        auto* cubemapObj = m_resMgr->getTexture(cubemap);
        RHI::ImageSubresourceRange cubeViewRange;
        cubeViewRange.aspectMask = RHI::ImageAspect::Color;
        cubeViewRange.baseMipLevel = 0;
        cubeViewRange.levelCount = 1;
        cubeViewRange.baseArrayLayer = 0;
        cubeViewRange.layerCount = 6;
        void* cubeViewKey = cubemapObj->createView(cubeViewRange, RHI::ImageViewType::TextureCube);

        // ── ★ 布局转换范围：覆盖全部 mip ──
        RHI::ImageSubresourceRange allMips;
        allMips.aspectMask = RHI::ImageAspect::Color;
        allMips.baseMipLevel = 0;
        allMips.levelCount = mipLevels;
        allMips.baseArrayLayer = 0;
        allMips.layerCount = 6;
        void* nativeView = cubemapObj->getNativeHandleFromView(cubeViewKey);

        // ── 创建 DescriptorPool ──
        RHI::DescriptorPoolDesc poolDesc;
        poolDesc.maxSets = 1;
        poolDesc.poolSizes = {
            { RHI::DescriptorType::CombinedImageSampler, 1 },
            { RHI::DescriptorType::StorageImage,         1 }
        };
        poolDesc.freeDescriptorSet = true;
        auto pool = m_resMgr->createDescriptorPool(poolDesc);

        // ── 创建 DescriptorSet 并绑定资源 ──
        RHI::DescriptorSetDesc setDesc;
        setDesc.descriptorSetLayout = descLayout;
        setDesc.descriptorPool = pool;
        auto descSet = m_resMgr->createDescriptorSet(setDesc);
        if (!descSet.isValid()) {
            cubemapObj->destroyView(cubeViewKey);
            return RHI::TextureHandle::Null();
        }

        auto* setObj = m_resMgr->getDescriptorSet(descSet);
        // binding 0: equirect 贴图 + 采样器
        setObj->writeTexture(0, 0,
            m_resMgr->getTexture(equirectTex),
            m_resMgr->getSampler(sampler),
            RHI::ImageLayout::ShaderReadOnly);
        // binding 1: 输出 cubemap，使用全层视图
        setObj->writeTextureCustomView(1, 0,
            nativeView, nullptr,
            RHI::ImageLayout::General);
        setObj->update();

        // ── 过渡整张 Cube 到 General 布局（所有 mip / 层）──
        {
            OneTimeCommandExecutor executor(m_rhi.get(), m_resMgr.get());
            auto* cmd = executor.get();

            cubemapObj->transitionLayout(
                RHI::ImageLayout::General,
                RHI::PipelineStage::TopOfPipe,
                RHI::PipelineStage::ComputeShader,
                static_cast<RHI::AccessFlags>(0),
                static_cast<RHI::AccessFlags>(RHI::AccessFlag::ShaderWrite),
                allMips);

            cmd->bindComputePipeline(m_resMgr->getPipeline(pipeline));
            cmd->bindDescriptorSets(RHI::PipelineBindPoint::Compute,
                m_resMgr->getPipelineLayout(plLayout), 0, { descSet }, {});

            uint32_t fs = faceSize;
            cmd->pushConstants(m_resMgr->getPipelineLayout(plLayout),
                RHI::ShaderStage::Compute, 0, sizeof(uint32_t), &fs);

            uint32_t gx = (faceSize + 15) / 16;
            uint32_t gy = (faceSize + 15) / 16;
            cmd->dispatch(gx, gy, 6);      // 同时处理 6 个面
        }

        m_rhi->waitIdle();

        // ── 过渡 mip 0 到 ShaderReadOnly（供 downsampler 读取）──
        {
            RHI::ImageSubresourceRange mip0Range;
            mip0Range.aspectMask = RHI::ImageAspect::Color;
            mip0Range.baseMipLevel = 0;
            mip0Range.levelCount = 1;
            mip0Range.baseArrayLayer = 0;
            mip0Range.layerCount = 6;
            cubemapObj->transitionLayout(
                RHI::ImageLayout::ShaderReadOnly,
                RHI::PipelineStage::ComputeShader,
                RHI::PipelineStage::ComputeShader,
                static_cast<RHI::AccessFlags>(RHI::AccessFlag::ShaderWrite),
                static_cast<RHI::AccessFlags>(RHI::AccessFlag::ShaderRead),
                mip0Range);
        }

        // ═══════════════════════════════════════════════
        // 生成源 Cubemap 的 mip 链
        //   用 cubemap_downsample.comp 在 3D 方向空间
        //   做 2×2 box filter，处理 cubemap 接缝、
        //   始终从 mip 0 读取避免累积误差
        // ═══════════════════════════════════════════════
        if (mipLevels > 1) {
            Assets::ShaderLoader dsLoader(m_resMgr);
            auto dsInfo = dsLoader.loadFromFile(
                "assets/shaders/ibl/cubemap_downsample.comp", RHI::ShaderStage::Compute);
            if (dsInfo) {
                auto dsShader = dsInfo->module;

                RHI::DescriptorSetLayoutDesc dsLayoutDesc;
                dsLayoutDesc.bindings = {
                    { 0, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Compute },
                    { 1, RHI::DescriptorType::StorageImage,         1, RHI::ShaderStage::Compute }
                };
                auto dsDescLayout = m_resMgr->createDescriptorSetLayout(dsLayoutDesc);

                RHI::PipelineLayoutDesc dsPlDesc;
                dsPlDesc.descriptorSetLayouts = { dsDescLayout };
                dsPlDesc.pushConstants = { { RHI::ShaderStage::Compute, 0, 8 } };
                auto dsPlLayout = m_resMgr->createPipelineLayout(dsPlDesc);

                RHI::ComputePipelineDesc dsCompDesc;
                dsCompDesc.computeShader = dsShader;
                dsCompDesc.pipelineLayoutHandle = dsPlLayout;
                auto dsPipeline = m_resMgr->createComputePipeline(dsCompDesc);

                RHI::SamplerDesc dsSampDesc;
                dsSampDesc.minFilter = RHI::SamplerFilter::Linear;
                dsSampDesc.magFilter = RHI::SamplerFilter::Linear;
                dsSampDesc.addressU = RHI::SamplerAddressMode::ClampToEdge;
                dsSampDesc.addressV = RHI::SamplerAddressMode::ClampToEdge;
                dsSampDesc.addressW = RHI::SamplerAddressMode::ClampToEdge;
                auto dsSampler = m_resMgr->createSampler(dsSampDesc);
                auto* dsSamplerObj = m_resMgr->getSampler(dsSampler);

                RHI::DescriptorPoolDesc dsPoolDesc;
                dsPoolDesc.maxSets = 64;
                dsPoolDesc.poolSizes = {
                    { RHI::DescriptorType::CombinedImageSampler, 64 },
                    { RHI::DescriptorType::StorageImage,         64 }
                };
                dsPoolDesc.freeDescriptorSet = false;  // 一次性使用，不需要释放
                auto dsPool = m_resMgr->createDescriptorPool(dsPoolDesc);

                // ── 仅含 mip 0 的 Cube 视图 ──
                //    sampler 视图若覆盖全部 mip，而 mip 1+ 处于 General，
                //    会与描述符声明的 ShaderReadOnly 冲突
                RHI::ImageSubresourceRange srcOnlyMip0;
                srcOnlyMip0.aspectMask = RHI::ImageAspect::Color;
                srcOnlyMip0.baseMipLevel = 0;
                srcOnlyMip0.levelCount = 1;
                srcOnlyMip0.baseArrayLayer = 0;
                srcOnlyMip0.layerCount = 6;
                void* srcMip0View = cubemapObj->createView(srcOnlyMip0, RHI::ImageViewType::TextureCube);
                void* srcMip0Native = cubemapObj->getNativeHandleFromView(srcMip0View);

                for (uint32_t mip = 1; mip < mipLevels; ++mip) {
                    uint32_t srcSize = faceSize >> (mip - 1);
                    uint32_t dstSize = srcSize / 2;
                    uint32_t gx = (dstSize + 15) / 16;
                    uint32_t gy = (dstSize + 15) / 16;

                    std::vector<void*> faceViews;
                    std::vector<RHI::DescriptorSetHandle> faceDescSets;
                    for (int f = 0; f < 6; ++f) {
                        RHI::ImageSubresourceRange faceRange;
                        faceRange.aspectMask = RHI::ImageAspect::Color;
                        faceRange.baseMipLevel = mip;
                        faceRange.levelCount = 1;
                        faceRange.baseArrayLayer = uint32_t(f);
                        faceRange.layerCount = 1;
                        void* vk = cubemapObj->createView(faceRange, RHI::ImageViewType::Texture2D);
                        void* nv = cubemapObj->getNativeHandleFromView(vk);
                        faceViews.push_back(vk);

                        RHI::DescriptorSetDesc setDesc;
                        setDesc.descriptorSetLayout = dsDescLayout;
                        setDesc.descriptorPool = dsPool;
                        auto dsSet = m_resMgr->createDescriptorSet(setDesc);
                        auto* setObj = m_resMgr->getDescriptorSet(dsSet);
                        setObj->writeTextureCustomView(0, 0, srcMip0Native, dsSamplerObj,
                            RHI::ImageLayout::ShaderReadOnly);
                        setObj->writeTextureCustomView(1, 0, nv, nullptr,
                            RHI::ImageLayout::General);
                        setObj->update();
                        faceDescSets.push_back(dsSet);
                    }

                    {
                        OneTimeCommandExecutor dsExec(m_rhi.get(), m_resMgr.get());
                        auto* dsCmd = dsExec.get();
                        dsCmd->bindComputePipeline(m_resMgr->getPipeline(dsPipeline));

                        for (int f = 0; f < 6; ++f) {
                            dsCmd->bindDescriptorSets(RHI::PipelineBindPoint::Compute,
                                m_resMgr->getPipelineLayout(dsPlLayout), 0,
                                { faceDescSets[f] }, {});
                            struct { int face; float srcSize; } pc;
                            pc.face = f; pc.srcSize = float(srcSize);
                            dsCmd->pushConstants(m_resMgr->getPipelineLayout(dsPlLayout),
                                RHI::ShaderStage::Compute, 0, sizeof(pc), &pc);
                            dsCmd->dispatch(gx, gy, 1);
                        }
                    }

                    for (auto& set : faceDescSets) m_resMgr->destroy(set);
                    for (auto* vk : faceViews) cubemapObj->destroyView(vk);

                    RHI::ImageSubresourceRange mipRange;
                    mipRange.aspectMask = RHI::ImageAspect::Color;
                    mipRange.baseMipLevel = mip;
                    mipRange.levelCount = 1;
                    mipRange.baseArrayLayer = 0;
                    mipRange.layerCount = 6;
                    cubemapObj->transitionLayout(
                        RHI::ImageLayout::ShaderReadOnly,
                        RHI::PipelineStage::ComputeShader,
                        RHI::PipelineStage::AllCommands,
                        static_cast<RHI::AccessFlags>(RHI::AccessFlag::ShaderWrite),
                        static_cast<RHI::AccessFlags>(RHI::AccessFlag::ShaderRead),
                        mipRange);
                }

                m_resMgr->destroy(dsPool);
                m_resMgr->destroy(dsPipeline);
                m_resMgr->destroy(dsShader);
                m_resMgr->destroy(dsPlLayout);
                m_resMgr->destroy(dsDescLayout);
                m_resMgr->destroy(dsSampler);
                cubemapObj->destroyView(srcMip0View);
            }
        }

        // ── 清理资源 ──
        cubemapObj->destroyView(cubeViewKey);
        m_resMgr->destroy(descSet);
        m_resMgr->destroy(pool);
        m_resMgr->destroy(pipeline);
        m_resMgr->destroy(computeShader);
        m_resMgr->destroy(plLayout);
        m_resMgr->destroy(descLayout);
        m_resMgr->destroy(sampler);

        return cubemap;
    }

    // ═══════════════════════════════════════════════
    // 计算管线：Irradiance Map
    // ═══════════════════════════════════════════════

    RHI::TextureHandle IBLBuilder::generateIrradianceMapCS(
        RHI::TextureHandle envCubemap,
        uint32_t            outputSize)
    {
        if (!envCubemap.isValid()) return RHI::TextureHandle::Null();

        // ── 加载 Shader ──
        Assets::ShaderLoader loader(m_resMgr);
        auto csInfo = loader.loadFromFile(
            "assets/shaders/ibl/irradiance_convolution.comp", RHI::ShaderStage::Compute);
        if (!csInfo) return RHI::TextureHandle::Null();
        auto cs = csInfo->module;

        // ── 创建 DescriptorSetLayout ──
        RHI::DescriptorSetLayoutDesc layoutDesc;
        layoutDesc.bindings = {
            { 0, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Compute },
            { 1, RHI::DescriptorType::StorageImage,         1, RHI::ShaderStage::Compute }
        };
        auto descLayout = m_resMgr->createDescriptorSetLayout(layoutDesc);

        // ── 创建 PipelineLayout ──
        RHI::PipelineLayoutDesc plDesc;
        plDesc.descriptorSetLayouts = { descLayout };
        plDesc.pushConstants = { { RHI::ShaderStage::Compute, 0, sizeof(uint32_t) } };
        auto plLayout = m_resMgr->createPipelineLayout(plDesc);

        // ── 创建 ComputePipeline ──
        RHI::ComputePipelineDesc compDesc;
        compDesc.computeShader = cs;
        compDesc.pipelineLayoutHandle = plLayout;
        auto pipeline = m_resMgr->createComputePipeline(compDesc);

        // ── 创建采样器 ──
        RHI::SamplerDesc sampDesc;
        sampDesc.minFilter = RHI::SamplerFilter::Linear;
        sampDesc.magFilter = RHI::SamplerFilter::Linear;
        sampDesc.addressU = RHI::SamplerAddressMode::ClampToEdge;
        sampDesc.addressV = RHI::SamplerAddressMode::ClampToEdge;
        auto sampler = m_resMgr->createSampler(sampDesc);

        // ── 创建输出纹理（Cube，6 层，storage 标记）──
        RHI::TextureDesc irradDesc;
        irradDesc.extent = { outputSize, outputSize, 1 };
        irradDesc.format = RHI::Format::RGBA32_Float;
        irradDesc.type = RHI::TextureType::TextureCube;
        irradDesc.mipLevels = 1;
        irradDesc.arrayLayers = 6;
        irradDesc.sampleCount = 1;
        irradDesc.flags = RHI::ImageCreateFlags::CubeCompatible;
        irradDesc.allowUnorderedAccess = true;
        irradDesc.allowRenderTarget = false;
        irradDesc.debugName = "IrradianceMap";
        auto irradMap = m_resMgr->createTexture(irradDesc);
        if (!irradMap.isValid()) return irradMap;

        // ── 创建覆盖全部 6 个面的 Cube 视图 ──
        auto* irradObj = m_resMgr->getTexture(irradMap);
        RHI::ImageSubresourceRange allLayers;
        allLayers.aspectMask = RHI::ImageAspect::Color;
        allLayers.baseMipLevel = 0;
        allLayers.levelCount = 1;
        allLayers.baseArrayLayer = 0;
        allLayers.layerCount = 6;
        void* cubeViewKey = irradObj->createView(allLayers, RHI::ImageViewType::TextureCube);
        void* nativeView = irradObj->getNativeHandleFromView(cubeViewKey);

        // ── 创建 DescriptorPool ──
        RHI::DescriptorPoolDesc poolDesc;
        poolDesc.maxSets = 1;
        poolDesc.poolSizes = {
            { RHI::DescriptorType::CombinedImageSampler, 1 },
            { RHI::DescriptorType::StorageImage,         1 }
        };
        poolDesc.freeDescriptorSet = true;
        auto pool = m_resMgr->createDescriptorPool(poolDesc);

        // ── 创建 DescriptorSet 并手动绑定两个资源 ──
        RHI::DescriptorSetDesc setDesc;
        setDesc.descriptorSetLayout = descLayout;
        setDesc.descriptorPool = pool;
        auto descSet = m_resMgr->createDescriptorSet(setDesc);
        if (!descSet.isValid()) {
            irradObj->destroyView(cubeViewKey);
            return RHI::TextureHandle::Null();
        }

        auto* setObj = m_resMgr->getDescriptorSet(descSet);
        // binding 0: 输入环境贴图 + 采样器
        setObj->writeTexture(0, 0,
            m_resMgr->getTexture(envCubemap),
            m_resMgr->getSampler(sampler),
            RHI::ImageLayout::ShaderReadOnly);
        // binding 1: 输出辐照度贴图，使用全层视图
        setObj->writeTextureCustomView(1, 0,
            nativeView, nullptr,
            RHI::ImageLayout::General);
        setObj->update();

        // ── 过渡整张 Cube 到 General 布局 ──
        RHI::ImageSubresourceRange range{};
        range.aspectMask = RHI::ImageAspect::Color;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 6;

        {
            OneTimeCommandExecutor executor(m_rhi.get(), m_resMgr.get());
            auto* cmd = executor.get();

            irradObj->transitionLayout(
                RHI::ImageLayout::General,
                RHI::PipelineStage::TopOfPipe,
                RHI::PipelineStage::ComputeShader,
                static_cast<RHI::AccessFlags>(0),
                static_cast<RHI::AccessFlags>(RHI::AccessFlag::ShaderWrite),
                range);

            cmd->bindComputePipeline(m_resMgr->getPipeline(pipeline));
            cmd->bindDescriptorSets(RHI::PipelineBindPoint::Compute,
                m_resMgr->getPipelineLayout(plLayout), 0, { descSet }, {});

            uint32_t fs = outputSize;
            cmd->pushConstants(m_resMgr->getPipelineLayout(plLayout),
                RHI::ShaderStage::Compute, 0, sizeof(uint32_t), &fs);

            uint32_t gx = (outputSize + 15) / 16;
            uint32_t gy = (outputSize + 15) / 16;
            cmd->dispatch(gx, gy, 6);  // 同时处理 6 个面
        }

        m_rhi->waitIdle();

        // ── 过渡回 ShaderReadOnly ──
        irradObj->transitionLayout(
            RHI::ImageLayout::ShaderReadOnly,
            RHI::PipelineStage::ComputeShader,
            RHI::PipelineStage::AllCommands,
            static_cast<RHI::AccessFlags>(RHI::AccessFlag::ShaderWrite),
            static_cast<RHI::AccessFlags>(RHI::AccessFlag::ShaderRead),
            range);

        // ── 清理资源 ──
        irradObj->destroyView(cubeViewKey);
        m_resMgr->destroy(descSet);
        m_resMgr->destroy(pool);
        m_resMgr->destroy(pipeline);
        m_resMgr->destroy(cs);
        m_resMgr->destroy(plLayout);
        m_resMgr->destroy(descLayout);
        m_resMgr->destroy(sampler);

        LOG_INFO("Irradiance map (compute) generated: {}x{}", outputSize, outputSize);
        return irradMap;
    }

    // ═══════════════════════════════════════════════
    // 计算管线：Prefiltered Map
    // ═══════════════════════════════════════════════

    RHI::TextureHandle IBLBuilder::generatePrefilteredMapCS(
        RHI::TextureHandle envCubemap,
        uint32_t            baseSize,
        uint32_t            mipLevels)
    {
        if (!envCubemap.isValid()) return RHI::TextureHandle::Null();

        // ── 加载 Shader ──
        Assets::ShaderLoader loader(m_resMgr);
        auto csInfo = loader.loadFromFile(
            "assets/shaders/ibl/prefilter_envmap.comp", RHI::ShaderStage::Compute);
        if (!csInfo) return RHI::TextureHandle::Null();
        auto cs = csInfo->module;

        // ── 创建 DescriptorSetLayout ──
        RHI::DescriptorSetLayoutDesc layoutDesc;
        layoutDesc.bindings = {
            { 0, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Compute },
            { 1, RHI::DescriptorType::StorageImage,         1, RHI::ShaderStage::Compute }
        };
        auto descLayout = m_resMgr->createDescriptorSetLayout(layoutDesc);

        // ── 创建 PipelineLayout ──
        RHI::PipelineLayoutDesc plDesc;
        plDesc.descriptorSetLayouts = { descLayout };
        plDesc.pushConstants = { { RHI::ShaderStage::Compute, 0, 16 } };
        auto plLayout = m_resMgr->createPipelineLayout(plDesc);

        // ── 创建 ComputePipeline ──
        RHI::ComputePipelineDesc compDesc;
        compDesc.computeShader = cs;
        compDesc.pipelineLayoutHandle = plLayout;
        auto pipeline = m_resMgr->createComputePipeline(compDesc);

        // ── 创建采样器（允许访问全部 mip）──
        RHI::SamplerDesc sampDesc;
        sampDesc.minFilter = RHI::SamplerFilter::Linear;
        sampDesc.magFilter = RHI::SamplerFilter::Linear;
        sampDesc.addressU = RHI::SamplerAddressMode::ClampToEdge;
        sampDesc.addressV = RHI::SamplerAddressMode::ClampToEdge;
        sampDesc.maxLod = 32.0f;   // 允许 access 源 cubemap 的全部 mip
        auto samplerHandle = m_resMgr->createSampler(sampDesc);

        // ── 创建输出纹理 ──
        RHI::TextureDesc prefDesc;
        prefDesc.extent = { baseSize, baseSize, 1 };
        prefDesc.format = RHI::Format::RGBA16_Float;
        prefDesc.type = RHI::TextureType::TextureCube;
        prefDesc.mipLevels = mipLevels;
        prefDesc.arrayLayers = 6;
        prefDesc.sampleCount = 1;
        prefDesc.flags = RHI::ImageCreateFlags::CubeCompatible;
        prefDesc.allowUnorderedAccess = true;
        prefDesc.allowRenderTarget = false;
        prefDesc.debugName = "PrefilteredEnvMap";
        auto prefilteredMap = m_resMgr->createTexture(prefDesc);
        if (!prefilteredMap.isValid()) return prefilteredMap;

        // ── 预分配 DescriptorPool（30 个 set）──
        uint32_t totalFaces = mipLevels * 6;
        RHI::DescriptorPoolDesc poolDesc;
        poolDesc.maxSets = totalFaces;
        poolDesc.poolSizes = {
            { RHI::DescriptorType::CombinedImageSampler, totalFaces },
            { RHI::DescriptorType::StorageImage,         totalFaces }
        };
        poolDesc.freeDescriptorSet = true;
        auto pool = m_resMgr->createDescriptorPool(poolDesc);

        // ── 获取对象指针 ──
        auto* envTexObj = m_resMgr->getTexture(envCubemap);
        auto* samplerObj = m_resMgr->getSampler(samplerHandle);
        auto* prefObj = m_resMgr->getTexture(prefilteredMap);

        // ── 预创建所有临时 View 和 DescriptorSet ──
        std::vector<void*>                     allTempViews;
        std::vector<RHI::DescriptorSetHandle>  allDescSets;

        for (uint32_t mip = 0; mip < mipLevels; ++mip) {
            for (int face = 0; face < 6; ++face) {
                RHI::ImageSubresourceRange viewRange;
                viewRange.aspectMask = RHI::ImageAspect::Color;
                viewRange.baseMipLevel = mip;
                viewRange.levelCount = 1;
                viewRange.baseArrayLayer = uint32_t(face);
                viewRange.layerCount = 1;
                void* viewKey = prefObj->createView(viewRange, RHI::ImageViewType::Texture2D);
                void* nativeView = prefObj->getNativeHandleFromView(viewKey);
                allTempViews.push_back(viewKey);

                RHI::DescriptorSetDesc setDesc;
                setDesc.descriptorSetLayout = descLayout;
                setDesc.descriptorPool = pool;
                auto descSet = m_resMgr->createDescriptorSet(setDesc);
                if (!descSet.isValid()) {
                    prefObj->destroyView(viewKey);
                    continue;
                }

                auto* setObj = m_resMgr->getDescriptorSet(descSet);
                setObj->writeTexture(0, 0,
                    envTexObj, samplerObj,
                    RHI::ImageLayout::ShaderReadOnly);

                setObj->writeTextureCustomView(1, 0,
                    nativeView, nullptr,
                    RHI::ImageLayout::General);

                setObj->update();

                allDescSets.push_back(descSet);
            }
        }

        // ── 准备 transition 范围 ──
        RHI::ImageSubresourceRange allMipsAndLayers{};
        allMipsAndLayers.aspectMask = RHI::ImageAspect::Color;
        allMipsAndLayers.baseMipLevel = 0;
        allMipsAndLayers.levelCount = mipLevels;
        allMipsAndLayers.baseArrayLayer = 0;
        allMipsAndLayers.layerCount = 6;

        // ═══════════════════════════════════════
        // 一次性命令
        // ═══════════════════════════════════════
        {
            OneTimeCommandExecutor executor(m_rhi.get(), m_resMgr.get());
            auto* cmd = executor.get();

            // ── 过渡所有 mips 到 General ──
            prefObj->transitionLayout(
                RHI::ImageLayout::General,
                RHI::PipelineStage::TopOfPipe,
                RHI::PipelineStage::ComputeShader,
                static_cast<RHI::AccessFlags>(0),
                static_cast<RHI::AccessFlags>(RHI::AccessFlag::ShaderWrite),
                allMipsAndLayers);

            // ── 逐 (mip, face) dispatch ──
            float sourceFaceSize = static_cast<float>(envTexObj->getExtent().width);
            int setIdx = 0;
            for (uint32_t mip = 0; mip < mipLevels; ++mip) {
                uint32_t mipSize = baseSize >> mip;
                float    roughness = float(mip) / float(mipLevels - 1);
                uint32_t gx = (mipSize + 15) / 16;
                uint32_t gy = (mipSize + 15) / 16;

                for (int face = 0; face < 6; ++face) {
                    cmd->bindComputePipeline(m_resMgr->getPipeline(pipeline));
                    cmd->bindDescriptorSets(RHI::PipelineBindPoint::Compute,
                        m_resMgr->getPipelineLayout(plLayout), 0,
                        { allDescSets[setIdx] }, {});

                    struct { int face; float faceSize; float roughness; float envResolution; } pc;
                    pc.face = face;
                    pc.faceSize = float(mipSize);
                    pc.roughness = roughness;
                    pc.envResolution = sourceFaceSize;
                    cmd->pushConstants(m_resMgr->getPipelineLayout(plLayout),
                        RHI::ShaderStage::Compute, 0, sizeof(pc), &pc);

                    cmd->dispatch(gx, gy, 1);
                    setIdx++;
                }
            }
        }
        // ✅ executor 析构 → 提交 + wait

        // ✅ 清理临时资源
        m_rhi->waitIdle();
        for (auto* vk : allTempViews) {
            prefObj->destroyView(vk);
        }
        allTempViews.clear();

        for (auto& set : allDescSets) {
            m_resMgr->destroy(set);
        }
        allDescSets.clear();

        // ── 过渡回 ShaderReadOnly ──
        prefObj->transitionLayout(
            RHI::ImageLayout::ShaderReadOnly,
            RHI::PipelineStage::ComputeShader,
            RHI::PipelineStage::AllCommands,
            static_cast<RHI::AccessFlags>(RHI::AccessFlag::ShaderWrite),
            static_cast<RHI::AccessFlags>(RHI::AccessFlag::ShaderRead),
            allMipsAndLayers);

        // ── 清理 ──
        m_resMgr->destroy(pool);
        m_resMgr->destroy(pipeline);
        m_resMgr->destroy(cs);
        m_resMgr->destroy(plLayout);
        m_resMgr->destroy(descLayout);
        m_resMgr->destroy(samplerHandle);

        LOG_INFO("Prefiltered map (compute) generated: {}x{} ({} mips)", baseSize, baseSize, mipLevels);
        return prefilteredMap;
    }

    // ═══════════════════════════════════════════════
    // 计算管线：BRDF LUT
    // ═══════════════════════════════════════════════

    RHI::TextureHandle IBLBuilder::generateBrdfLutCS(uint32_t size) {
        Assets::ShaderLoader loader(m_resMgr);
        auto csInfo = loader.loadFromFile(
            "assets/shaders/ibl/brdf_lut.comp", RHI::ShaderStage::Compute);
        if (!csInfo) return RHI::TextureHandle::Null();
        auto cs = csInfo->module;

        RHI::DescriptorSetLayoutDesc layoutDesc;
        layoutDesc.bindings = {
            { 0, RHI::DescriptorType::StorageImage, 1, RHI::ShaderStage::Compute }
        };
        auto descLayout = m_resMgr->createDescriptorSetLayout(layoutDesc);

        RHI::PipelineLayoutDesc plDesc;
        plDesc.descriptorSetLayouts = { descLayout };
        auto plLayout = m_resMgr->createPipelineLayout(plDesc);

        RHI::ComputePipelineDesc compDesc;
        compDesc.computeShader = cs;
        compDesc.pipelineLayoutHandle = plLayout;
        auto pipeline = m_resMgr->createComputePipeline(compDesc);

        RHI::DescriptorPoolDesc poolDesc;
        poolDesc.maxSets = 1;
        poolDesc.poolSizes = { { RHI::DescriptorType::StorageImage, 1 } };
        poolDesc.freeDescriptorSet = true;
        auto pool = m_resMgr->createDescriptorPool(poolDesc);

        RHI::TextureDesc texDesc;
        texDesc.extent = { size, size, 1 };
        texDesc.format = RHI::Format::RG16_Float;
        texDesc.type = RHI::TextureType::Texture2D;
        texDesc.mipLevels = 1;
        texDesc.arrayLayers = 1;
        texDesc.sampleCount = 1;
        texDesc.allowUnorderedAccess = true;
        texDesc.allowRenderTarget = false;
        texDesc.debugName = "BrdfLUT";
        auto brdfLut = m_resMgr->createTexture(texDesc);
        if (!brdfLut.isValid()) return brdfLut;

        RHI::DescriptorSetDesc setDesc;
        setDesc.descriptorSetLayout = descLayout;
        setDesc.descriptorPool = pool;
        auto descSet = m_resMgr->createDescriptorSet(setDesc);
        m_resMgr->getDescriptorSet(descSet)->writeTexture(0, 0,
            m_resMgr->getTexture(brdfLut), nullptr, RHI::ImageLayout::General);

        RHI::ImageSubresourceRange range{};
        range.aspectMask = RHI::ImageAspect::Color;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;

        {
            OneTimeCommandExecutor executor(m_rhi.get(), m_resMgr.get());
            auto* cmd = executor.get();

            m_resMgr->getTexture(brdfLut)->transitionLayout(
                RHI::ImageLayout::General,
                RHI::PipelineStage::TopOfPipe,
                RHI::PipelineStage::ComputeShader,
                static_cast<RHI::AccessFlags>(0),
                static_cast<RHI::AccessFlags>(RHI::AccessFlag::ShaderWrite),
                range);

            m_resMgr->getDescriptorSet(descSet)->update();

            cmd->bindComputePipeline(m_resMgr->getPipeline(pipeline));
            cmd->bindDescriptorSets(RHI::PipelineBindPoint::Compute,
                m_resMgr->getPipelineLayout(plLayout), 0, { descSet }, {});

            uint32_t gx = (size + 15) / 16;
            uint32_t gy = (size + 15) / 16;
            cmd->dispatch(gx, gy, 1);
        }

        m_rhi->waitIdle();
        m_resMgr->getTexture(brdfLut)->transitionLayout(
            RHI::ImageLayout::ShaderReadOnly,
            RHI::PipelineStage::ComputeShader,
            RHI::PipelineStage::AllCommands,
            static_cast<RHI::AccessFlags>(RHI::AccessFlag::ShaderWrite),
            static_cast<RHI::AccessFlags>(RHI::AccessFlag::ShaderRead),
            range);

        m_resMgr->destroy(descSet);
        m_resMgr->destroy(pool);
        m_resMgr->destroy(pipeline);
        m_resMgr->destroy(cs);
        m_resMgr->destroy(plLayout);
        m_resMgr->destroy(descLayout);
        LOG_INFO("BRDF LUT (compute) generated: {}x{}", size, size);
        return brdfLut;
    }

    // ═══════════════════════════════════════════════
    // 便捷方法
    // ═══════════════════════════════════════════════

    RHI::TextureHandle IBLBuilder::buildEnvCubemap(
        const std::string& hdrPath,
        uint32_t            faceSize)
    {
        return buildEnvCubemap(hdrPath, faceSize, true);  // 默认用计算管线
    }

    RHI::TextureHandle IBLBuilder::buildEnvCubemap(
        const std::string& hdrPath,
        uint32_t            faceSize,
        bool                useComputeShader)
    {
        int w, h, c;
        float* pixels = stbi_loadf(hdrPath.c_str(), &w, &h, &c, STBI_rgb_alpha);
        if (!pixels) {
            LOG_ERROR("Failed to load HDR: {}", hdrPath);
            return RHI::TextureHandle::Null();
        }

        RHI::TextureDesc equirectDesc;
        equirectDesc.extent = { uint32_t(w), uint32_t(h), 1 };
        equirectDesc.format = RHI::Format::RGBA32_Float;
        equirectDesc.type = RHI::TextureType::Texture2D;
        equirectDesc.mipLevels = 1;
        equirectDesc.arrayLayers = 1;
        equirectDesc.sampleCount = 1;
        equirectDesc.debugName = "EquirectTemp";
        auto equirectTex = m_resMgr->createTexture(equirectDesc);

        auto* texObj = m_resMgr->getTexture(equirectTex);
        if (texObj) {
            RHI::ImageSubresourceRange range{};
            range.aspectMask = RHI::ImageAspect::Color;
            range.baseMipLevel = 0;
            range.levelCount = 1;
            range.baseArrayLayer = 0;
            range.layerCount = 1;
            texObj->update(pixels, w * h * 4 * sizeof(float), range);
            texObj->transitionLayout(
                RHI::ImageLayout::ShaderReadOnly,
                RHI::PipelineStage::Transfer,
                RHI::PipelineStage::FragmentShader,
                static_cast<RHI::AccessFlags>(RHI::AccessFlag::TransferWrite),
                static_cast<RHI::AccessFlags>(RHI::AccessFlag::ShaderRead),
                range);
        }
        stbi_image_free(pixels);

        RHI::TextureHandle cubemap = useComputeShader? equirectToCubemapCS(equirectTex, faceSize): equirectToCubemap(equirectTex, faceSize);

        m_resMgr->destroy(equirectTex);
        return cubemap;
    }

} // namespace StarryEngine::Assets