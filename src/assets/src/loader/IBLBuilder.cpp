#include <assets/loader/IBLBuilder.hpp>
#include <assets/loader/TextureLoader.hpp>
#include <assets/loader/ShaderLoader.hpp>
#include <logging/Logger.hpp>
#include <stb_image.h>
#include <stb_image_write.h>

namespace StarryEngine::Assets {

    static RHI::RenderPassHandle createSimpleColorRenderPass(
        RHI::ResourceManager* resMgr,
        RHI::Format colorFormat,
        RHI::AttachmentLoadOp loadOp = RHI::AttachmentLoadOp::DontCare,
        RHI::AttachmentStoreOp storeOp = RHI::AttachmentStoreOp::Store,
        RHI::ImageLayout finalLayout = RHI::ImageLayout::ShaderReadOnly){

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
        const std::vector<RHI::DynamicState>& dynamicStates ={ RHI::DynamicState::Viewport, RHI::DynamicState::Scissor }){

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

    IBLBuilder::IBLBuilder(std::shared_ptr<RHI::ResourceManager> resMgr,std::shared_ptr<RHI::IRHI> rhi)
        : m_resMgr(std::move(resMgr)), m_rhi(std::move(rhi)) {

        Assets::ShaderLoader shaderLoader(m_resMgr);
        auto vsInfo = shaderLoader.loadFromFile("assets/shaders/ibl/equirect_to_cubemap.vert", RHI::ShaderStage::Vertex);
        auto fsInfo = shaderLoader.loadFromFile("assets/shaders/ibl/equirect_to_cubemap.frag", RHI::ShaderStage::Fragment);

        if (vsInfo && fsInfo) {
            m_vs = vsInfo->module;
            m_fs = fsInfo->module;
        } else {
            LOG_WARN("IBLBuilder: default fullscreen shaders not loaded");
        }

        RHI::DescriptorSetLayoutDesc layoutDesc;
        layoutDesc.bindings = {
            { 0, RHI::DescriptorType::CombinedImageSampler, 1,
              RHI::ShaderStage::Fragment }
        };
        m_descLayout = m_resMgr->createDescriptorSetLayout(layoutDesc);

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

    RHI::DescriptorSetHandle IBLBuilder::createDescriptorSet(
        RHI::TextureHandle      texture,
        RHI::SamplerHandle      sampler,
        RHI::DescriptorSetLayoutHandle layout,
        RHI::DescriptorPoolHandle      pool){
        RHI::DescriptorSetDesc setDesc;
        setDesc.descriptorSetLayout = layout;
        setDesc.descriptorPool = pool;

        auto descSet = m_resMgr->createDescriptorSet(setDesc);
        if (!descSet.isValid()) return descSet;

        auto* setObj = m_resMgr->getDescriptorSet(descSet);
        auto* texObj = m_resMgr->getTexture(texture);
        auto* samplerObj = m_resMgr->getSampler(sampler);
        if (setObj && texObj && samplerObj) {
            setObj->writeTexture(0, 0, texObj, samplerObj,RHI::ImageLayout::ShaderReadOnly);setObj->update();
        }
        return descSet;
    }

    RHI::DescriptorSetHandle IBLBuilder::createComputeDescriptorSet(
        RHI::TextureHandle      inputTex,
        RHI::SamplerHandle      sampler,
        RHI::TextureHandle      outputTex,
        RHI::DescriptorSetLayoutHandle layout,
        RHI::DescriptorPoolHandle      pool,
        RHI::ImageLayout        outputLayout){

        RHI::DescriptorSetDesc setDesc;
        setDesc.descriptorSetLayout = layout;
        setDesc.descriptorPool = pool;
        auto descSet = m_resMgr->createDescriptorSet(setDesc);
        if (!descSet.isValid()) return descSet;

        auto* setObj = m_resMgr->getDescriptorSet(descSet);
        setObj->writeTexture(0, 0,m_resMgr->getTexture(inputTex),m_resMgr->getSampler(sampler),RHI::ImageLayout::ShaderReadOnly);
        setObj->writeTexture(1, 0,m_resMgr->getTexture(outputTex),nullptr,outputLayout);
        return descSet;
    }

    RHI::TextureHandle IBLBuilder::equirectToCubemap(RHI::TextureHandle equirectTex,uint32_t faceSize){

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
            fbDesc.extent = { faceSize, faceSize };
            fbDesc.layers = 1;
            fbDesc.nativeAttachments = { nativeView };
            framebuffers[face] = m_resMgr->createFramebuffer(renderPass, {}, fbDesc);
        }

        {
            OneTimeCommandExecutor executor(m_rhi.get(), m_resMgr.get());
            auto* cmd = executor.get();

            for (int face = 0; face < 6; ++face) {
                RHI::RenderPassBeginInfo rpBegin;
                rpBegin.renderPass = renderPass;
                rpBegin.framebuffer = framebuffers[face];
                rpBegin.renderArea = { {0, 0}, {faceSize, faceSize} };
                rpBegin.clearValues = { {0.0f, 0.0f, 0.0f, 0.0f} };
                cmd->beginRenderPass(rpBegin, RHI::SubpassContents::Inline);
                cmd->bindGraphicPipeline(pipeline);
                cmd->bindDescriptorSets(RHI::PipelineBindPoint::Graphics, m_pipelineLayout,
                    0, { descSet }, {});
                struct { int face; float faceSize; } pc = { face, float(faceSize) };
                cmd->pushConstants(m_pipelineLayout, RHI::ShaderStage::Fragment,
                    0, sizeof(pc), &pc);
                cmd->setViewport({ 0.0f, 0.0f, float(faceSize), float(faceSize), 0.0f, 1.0f });
                cmd->setScissor({ {0, 0}, {faceSize, faceSize} });
                cmd->draw(3, 1, 0, 0);
                cmd->endRenderPass();
            }

        }

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
                        RHI::AccessFlag::None,
                        RHI::AccessFlag::ShaderWrite,
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
                        dsCmd->bindComputePipeline(dsPipeline);

                        for (int f = 0; f < 6; ++f) {
                            dsCmd->bindDescriptorSets(RHI::PipelineBindPoint::Compute,
                                dsPlLayout, 0,
                                { faceDescSets[f] }, {});
                            struct { int face; float srcSize; } pc;
                            pc.face = f; pc.srcSize = float(srcSize);
                            dsCmd->pushConstants(dsPlLayout,
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
                        RHI::AccessFlag::ShaderWrite,
                        RHI::AccessFlag::ShaderRead,
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

    RHI::TextureHandle IBLBuilder::generateIrradianceMap(RHI::TextureHandle envCubemap,uint32_t outputSize){

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
            fbDesc.extent = { outputSize, outputSize };
            fbDesc.layers = 1;
            fbDesc.nativeAttachments = { nv };
            framebuffers[face] = m_resMgr->createFramebuffer(renderPass, {}, fbDesc);
        }

        {
            OneTimeCommandExecutor executor(m_rhi.get(), m_resMgr.get());
            auto* cmd = executor.get();
            for (int face = 0; face < 6; ++face) {
                RHI::RenderPassBeginInfo rpBegin;
                rpBegin.renderPass = renderPass;
                rpBegin.framebuffer = framebuffers[face];
                rpBegin.renderArea = { {0, 0}, {outputSize, outputSize} };
                rpBegin.clearValues = { {0.0f, 0.0f, 0.0f, 0.0f} };
                cmd->beginRenderPass(rpBegin, RHI::SubpassContents::Inline);
                cmd->bindGraphicPipeline(pipeline);
                cmd->bindDescriptorSets(RHI::PipelineBindPoint::Graphics, plLayout, 0, { descSet }, {});
                struct { int face; float faceSize; } pc = { face, float(outputSize) };
                cmd->pushConstants(plLayout, RHI::ShaderStage::Fragment, 0, sizeof(pc), &pc);
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

    RHI::TextureHandle IBLBuilder::generatePrefilteredMap(
        RHI::TextureHandle envCubemap,
        uint32_t            baseSize,
        uint32_t            mipLevels){

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
                    fbDesc.extent = { mipSize, mipSize };
                    fbDesc.layers = 1;
                    fbDesc.nativeAttachments = { nv };
                    auto fb = m_resMgr->createFramebuffer(renderPass, {}, fbDesc);
                    tempFBs.push_back(fb);

                    RHI::RenderPassBeginInfo rpBegin;
                    rpBegin.renderPass = renderPass;
                    rpBegin.framebuffer = fb;
                    rpBegin.renderArea = { {0, 0}, {mipSize, mipSize} };
                    rpBegin.clearValues = { {0.0f, 0.0f, 0.0f, 0.0f} };
                    cmd->beginRenderPass(rpBegin, RHI::SubpassContents::Inline);
                    cmd->bindGraphicPipeline(pipeline);
                    cmd->bindDescriptorSets(RHI::PipelineBindPoint::Graphics, plLayout, 0, { descSet }, {});
                    struct { int face; float faceSize; float roughness; float envResolution; } pc;
                    pc.face = face; pc.faceSize = float(mipSize); pc.roughness = roughness;
                    pc.envResolution = sourceFaceSize;
                    cmd->pushConstants(plLayout, RHI::ShaderStage::Fragment, 0, sizeof(pc), &pc);
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
        fbDesc.extent = { size, size };
        fbDesc.layers = 1;
        fbDesc.nativeAttachments = { nv };
        auto framebuffer = m_resMgr->createFramebuffer(renderPass, {}, fbDesc);

        {
            OneTimeCommandExecutor executor(m_rhi.get(), m_resMgr.get());
            auto* cmd = executor.get();
            RHI::RenderPassBeginInfo rpBegin;
            rpBegin.renderPass = renderPass;
            rpBegin.framebuffer = framebuffer;
            rpBegin.renderArea = { {0, 0}, {size, size} };
            rpBegin.clearValues = { {0.0f, 0.0f, 0.0f, 0.0f} };
            cmd->beginRenderPass(rpBegin, RHI::SubpassContents::Inline);
            cmd->bindGraphicPipeline(pipeline);
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

    RHI::TextureHandle IBLBuilder::equirectToCubemapCS(RHI::TextureHandle equirectTex,uint32_t faceSize){

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
        cubemapDesc.allowUnorderedAccess = true;
        cubemapDesc.allowRenderTarget = false;
        cubemapDesc.debugName = "EnvCubemap";
        auto cubemap = m_resMgr->createTexture(cubemapDesc);
        if (!cubemap.isValid()) return cubemap;

        // ── 加载 Compute Shader ──
        Assets::ShaderLoader shaderLoader(m_resMgr);
        auto csInfo = shaderLoader.loadFromFile("assets/shaders/ibl/equirect_to_cubemap.comp", RHI::ShaderStage::Compute);
        if (!csInfo) return RHI::TextureHandle::Null();
        auto computeShader = csInfo->module;

        RHI::DescriptorSetLayoutDesc layoutDesc;
        layoutDesc.bindings = {
            { 0, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Compute },
            { 1, RHI::DescriptorType::StorageImage,         1, RHI::ShaderStage::Compute }
        };
        auto descLayout = m_resMgr->createDescriptorSetLayout(layoutDesc);

        RHI::PipelineLayoutDesc plDesc;
        plDesc.descriptorSetLayouts = { descLayout };
        plDesc.pushConstants = { { RHI::ShaderStage::Compute, 0, sizeof(uint32_t) } };
        auto plLayout = m_resMgr->createPipelineLayout(plDesc);

        RHI::ComputePipelineDesc compDesc;
        compDesc.computeShader = computeShader;
        compDesc.pipelineLayoutHandle = plLayout;
        auto pipeline = m_resMgr->createComputePipeline(compDesc);

        RHI::SamplerDesc sampDesc;
        sampDesc.minFilter = RHI::SamplerFilter::Linear;
        sampDesc.magFilter = RHI::SamplerFilter::Linear;
        sampDesc.addressU = RHI::SamplerAddressMode::ClampToEdge;
        sampDesc.addressV = RHI::SamplerAddressMode::ClampToEdge;
        auto sampler = m_resMgr->createSampler(sampDesc);

        auto* cubemapObj = m_resMgr->getTexture(cubemap);
        RHI::ImageSubresourceRange cubeViewRange;
        cubeViewRange.aspectMask = RHI::ImageAspect::Color;
        cubeViewRange.baseMipLevel = 0;
        cubeViewRange.levelCount = 1;
        cubeViewRange.baseArrayLayer = 0;
        cubeViewRange.layerCount = 6;
        void* cubeViewKey = cubemapObj->createView(cubeViewRange, RHI::ImageViewType::TextureCube);

        RHI::ImageSubresourceRange allMips;
        allMips.aspectMask = RHI::ImageAspect::Color;
        allMips.baseMipLevel = 0;
        allMips.levelCount = mipLevels;
        allMips.baseArrayLayer = 0;
        allMips.layerCount = 6;
        void* nativeView = cubemapObj->getNativeHandleFromView(cubeViewKey);

        RHI::DescriptorPoolDesc poolDesc;
        poolDesc.maxSets = 1;
        poolDesc.poolSizes = {
            { RHI::DescriptorType::CombinedImageSampler, 1 },
            { RHI::DescriptorType::StorageImage,         1 }
        };
        poolDesc.freeDescriptorSet = true;
        auto pool = m_resMgr->createDescriptorPool(poolDesc);

        RHI::DescriptorSetDesc setDesc;
        setDesc.descriptorSetLayout = descLayout;
        setDesc.descriptorPool = pool;
        auto descSet = m_resMgr->createDescriptorSet(setDesc);
        if (!descSet.isValid()) {
            cubemapObj->destroyView(cubeViewKey);
            return RHI::TextureHandle::Null();
        }

        auto* setObj = m_resMgr->getDescriptorSet(descSet);
        setObj->writeTexture(0, 0,m_resMgr->getTexture(equirectTex),m_resMgr->getSampler(sampler),RHI::ImageLayout::ShaderReadOnly);
        setObj->writeTextureCustomView(1, 0,nativeView, nullptr,RHI::ImageLayout::General);
        setObj->update();

        {
            OneTimeCommandExecutor executor(m_rhi.get(), m_resMgr.get());
            auto* cmd = executor.get();

            cubemapObj->transitionLayout(
                RHI::ImageLayout::General,
                RHI::PipelineStage::TopOfPipe,
                RHI::PipelineStage::ComputeShader,
                RHI::AccessFlag::None,
                RHI::AccessFlag::ShaderWrite,
                allMips);

            cmd->bindComputePipeline(pipeline);
            cmd->bindDescriptorSets(RHI::PipelineBindPoint::Compute,
                plLayout, 0, { descSet }, {});

            uint32_t fs = faceSize;
            cmd->pushConstants(plLayout,
                RHI::ShaderStage::Compute, 0, sizeof(uint32_t), &fs);

            uint32_t gx = (faceSize + 15) / 16;
            uint32_t gy = (faceSize + 15) / 16;
            cmd->dispatch(gx, gy, 6);     
        }

        m_rhi->waitIdle();

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
                RHI::AccessFlag::ShaderWrite,
                RHI::AccessFlag::ShaderRead,
                mip0Range);
        }

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
                        dsCmd->bindComputePipeline(dsPipeline);

                        for (int f = 0; f < 6; ++f) {
                            dsCmd->bindDescriptorSets(RHI::PipelineBindPoint::Compute,
                                dsPlLayout, 0,
                                { faceDescSets[f] }, {});
                            struct { int face; float srcSize; } pc;
                            pc.face = f; pc.srcSize = float(srcSize);
                            dsCmd->pushConstants(dsPlLayout,
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
                        RHI::AccessFlag::ShaderWrite,
                        RHI::AccessFlag::ShaderRead,
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

    RHI::TextureHandle IBLBuilder::generateIrradianceMapCS(RHI::TextureHandle envCubemap,uint32_t outputSize){

        if (!envCubemap.isValid()) return RHI::TextureHandle::Null();

        Assets::ShaderLoader loader(m_resMgr);
        auto csInfo = loader.loadFromFile(
            "assets/shaders/ibl/irradiance_convolution.comp", RHI::ShaderStage::Compute);
        if (!csInfo) return RHI::TextureHandle::Null();
        auto cs = csInfo->module;

        RHI::DescriptorSetLayoutDesc layoutDesc;
        layoutDesc.bindings = {
            { 0, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Compute },
            { 1, RHI::DescriptorType::StorageImage,         1, RHI::ShaderStage::Compute }
        };
        auto descLayout = m_resMgr->createDescriptorSetLayout(layoutDesc);

        RHI::PipelineLayoutDesc plDesc;
        plDesc.descriptorSetLayouts = { descLayout };
        plDesc.pushConstants = { { RHI::ShaderStage::Compute, 0, sizeof(uint32_t) } };
        auto plLayout = m_resMgr->createPipelineLayout(plDesc);

        RHI::ComputePipelineDesc compDesc;
        compDesc.computeShader = cs;
        compDesc.pipelineLayoutHandle = plLayout;
        auto pipeline = m_resMgr->createComputePipeline(compDesc);

        RHI::SamplerDesc sampDesc;
        sampDesc.minFilter = RHI::SamplerFilter::Linear;
        sampDesc.magFilter = RHI::SamplerFilter::Linear;
        sampDesc.addressU = RHI::SamplerAddressMode::ClampToEdge;
        sampDesc.addressV = RHI::SamplerAddressMode::ClampToEdge;
        auto sampler = m_resMgr->createSampler(sampDesc);

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

        auto* irradObj = m_resMgr->getTexture(irradMap);
        RHI::ImageSubresourceRange allLayers;
        allLayers.aspectMask = RHI::ImageAspect::Color;
        allLayers.baseMipLevel = 0;
        allLayers.levelCount = 1;
        allLayers.baseArrayLayer = 0;
        allLayers.layerCount = 6;
        void* cubeViewKey = irradObj->createView(allLayers, RHI::ImageViewType::TextureCube);
        void* nativeView = irradObj->getNativeHandleFromView(cubeViewKey);

        RHI::DescriptorPoolDesc poolDesc;
        poolDesc.maxSets = 1;
        poolDesc.poolSizes = {
            { RHI::DescriptorType::CombinedImageSampler, 1 },
            { RHI::DescriptorType::StorageImage,         1 }
        };
        poolDesc.freeDescriptorSet = true;
        auto pool = m_resMgr->createDescriptorPool(poolDesc);

        RHI::DescriptorSetDesc setDesc;
        setDesc.descriptorSetLayout = descLayout;
        setDesc.descriptorPool = pool;
        auto descSet = m_resMgr->createDescriptorSet(setDesc);
        if (!descSet.isValid()) {
            irradObj->destroyView(cubeViewKey);
            return RHI::TextureHandle::Null();
        }

        auto* setObj = m_resMgr->getDescriptorSet(descSet);
        setObj->writeTexture(0, 0,m_resMgr->getTexture(envCubemap),m_resMgr->getSampler(sampler),RHI::ImageLayout::ShaderReadOnly);
        setObj->writeTextureCustomView(1, 0,nativeView, nullptr,RHI::ImageLayout::General);
        setObj->update();

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
                RHI::AccessFlag::None,
                RHI::AccessFlag::ShaderWrite, range);

            cmd->bindComputePipeline(pipeline);
            cmd->bindDescriptorSets(RHI::PipelineBindPoint::Compute,plLayout, 0, { descSet }, {});

            uint32_t fs = outputSize;
            cmd->pushConstants(plLayout,RHI::ShaderStage::Compute, 0, sizeof(uint32_t), &fs);

            uint32_t gx = (outputSize + 15) / 16;
            uint32_t gy = (outputSize + 15) / 16;
            cmd->dispatch(gx, gy, 6);  
        }

        m_rhi->waitIdle();

        irradObj->transitionLayout(
            RHI::ImageLayout::ShaderReadOnly,
            RHI::PipelineStage::ComputeShader,
            RHI::PipelineStage::AllCommands,
            RHI::AccessFlag::ShaderWrite,
            RHI::AccessFlag::ShaderRead,range);

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

    RHI::TextureHandle IBLBuilder::generatePrefilteredMapCS(
        RHI::TextureHandle envCubemap,
        uint32_t            baseSize,
        uint32_t            mipLevels){

        if (!envCubemap.isValid()) return RHI::TextureHandle::Null();

        Assets::ShaderLoader loader(m_resMgr);
        auto csInfo = loader.loadFromFile(
            "assets/shaders/ibl/prefilter_envmap.comp", RHI::ShaderStage::Compute);
        if (!csInfo) return RHI::TextureHandle::Null();
        auto cs = csInfo->module;

        RHI::DescriptorSetLayoutDesc layoutDesc;
        layoutDesc.bindings = {
            { 0, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Compute },
            { 1, RHI::DescriptorType::StorageImage,         1, RHI::ShaderStage::Compute }
        };
        auto descLayout = m_resMgr->createDescriptorSetLayout(layoutDesc);

        RHI::PipelineLayoutDesc plDesc;
        plDesc.descriptorSetLayouts = { descLayout };
        plDesc.pushConstants = { { RHI::ShaderStage::Compute, 0, 16 } };
        auto plLayout = m_resMgr->createPipelineLayout(plDesc);

        RHI::ComputePipelineDesc compDesc;
        compDesc.computeShader = cs;
        compDesc.pipelineLayoutHandle = plLayout;
        auto pipeline = m_resMgr->createComputePipeline(compDesc);

        RHI::SamplerDesc sampDesc;
        sampDesc.minFilter = RHI::SamplerFilter::Linear;
        sampDesc.magFilter = RHI::SamplerFilter::Linear;
        sampDesc.addressU = RHI::SamplerAddressMode::ClampToEdge;
        sampDesc.addressV = RHI::SamplerAddressMode::ClampToEdge;
        sampDesc.maxLod = 32.0f;   // 允许 access 源 cubemap 的全部 mip
        auto samplerHandle = m_resMgr->createSampler(sampDesc);

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

        uint32_t totalFaces = mipLevels * 6;
        RHI::DescriptorPoolDesc poolDesc;
        poolDesc.maxSets = totalFaces;
        poolDesc.poolSizes = {
            { RHI::DescriptorType::CombinedImageSampler, totalFaces },
            { RHI::DescriptorType::StorageImage,         totalFaces }
        };
        poolDesc.freeDescriptorSet = true;
        auto pool = m_resMgr->createDescriptorPool(poolDesc);

        auto* envTexObj = m_resMgr->getTexture(envCubemap);
        auto* samplerObj = m_resMgr->getSampler(samplerHandle);
        auto* prefObj = m_resMgr->getTexture(prefilteredMap);

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

        RHI::ImageSubresourceRange allMipsAndLayers{};
        allMipsAndLayers.aspectMask = RHI::ImageAspect::Color;
        allMipsAndLayers.baseMipLevel = 0;
        allMipsAndLayers.levelCount = mipLevels;
        allMipsAndLayers.baseArrayLayer = 0;
        allMipsAndLayers.layerCount = 6;

        {
            OneTimeCommandExecutor executor(m_rhi.get(), m_resMgr.get());
            auto* cmd = executor.get();

            prefObj->transitionLayout(
                RHI::ImageLayout::General,
                RHI::PipelineStage::TopOfPipe,
                RHI::PipelineStage::ComputeShader,
                RHI::AccessFlag::None,
                RHI::AccessFlag::ShaderWrite,
                allMipsAndLayers);

            float sourceFaceSize = static_cast<float>(envTexObj->getExtent().width);
            int setIdx = 0;
            for (uint32_t mip = 0; mip < mipLevels; ++mip) {
                uint32_t mipSize = baseSize >> mip;
                float    roughness = float(mip) / float(mipLevels - 1);
                uint32_t gx = (mipSize + 15) / 16;
                uint32_t gy = (mipSize + 15) / 16;

                for (int face = 0; face < 6; ++face) {
                    cmd->bindComputePipeline(pipeline);
                    cmd->bindDescriptorSets(RHI::PipelineBindPoint::Compute,
                        plLayout, 0,
                        { allDescSets[setIdx] }, {});

                    struct { int face; float faceSize; float roughness; float envResolution; } pc;
                    pc.face = face;
                    pc.faceSize = float(mipSize);
                    pc.roughness = roughness;
                    pc.envResolution = sourceFaceSize;
                    cmd->pushConstants(plLayout,
                        RHI::ShaderStage::Compute, 0, sizeof(pc), &pc);

                    cmd->dispatch(gx, gy, 1);
                    setIdx++;
                }
            }
        }

        m_rhi->waitIdle();
        for (auto* vk : allTempViews) {
            prefObj->destroyView(vk);
        }
        allTempViews.clear();

        for (auto& set : allDescSets) {
            m_resMgr->destroy(set);
        }
        allDescSets.clear();

        prefObj->transitionLayout(
            RHI::ImageLayout::ShaderReadOnly,
            RHI::PipelineStage::ComputeShader,
            RHI::PipelineStage::AllCommands,
            RHI::AccessFlag::ShaderWrite,
            RHI::AccessFlag::ShaderRead,
            allMipsAndLayers);

        m_resMgr->destroy(pool);
        m_resMgr->destroy(pipeline);
        m_resMgr->destroy(cs);
        m_resMgr->destroy(plLayout);
        m_resMgr->destroy(descLayout);
        m_resMgr->destroy(samplerHandle);

        LOG_INFO("Prefiltered map (compute) generated: {}x{} ({} mips)", baseSize, baseSize, mipLevels);
        return prefilteredMap;
    }

    RHI::TextureHandle IBLBuilder::generateBrdfLutCS(uint32_t size) {

        Assets::ShaderLoader loader(m_resMgr);
        auto csInfo = loader.loadFromFile("assets/shaders/ibl/brdf_lut.comp", RHI::ShaderStage::Compute);

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
                RHI::AccessFlag::None,
                RHI::AccessFlag::ShaderWrite,
                range);

            m_resMgr->getDescriptorSet(descSet)->update();

            cmd->bindComputePipeline(pipeline);
            cmd->bindDescriptorSets(RHI::PipelineBindPoint::Compute,
                plLayout, 0, { descSet }, {});

            uint32_t gx = (size + 15) / 16;
            uint32_t gy = (size + 15) / 16;
            cmd->dispatch(gx, gy, 1);
        }

        m_rhi->waitIdle();
        m_resMgr->getTexture(brdfLut)->transitionLayout(
            RHI::ImageLayout::ShaderReadOnly,
            RHI::PipelineStage::ComputeShader,
            RHI::PipelineStage::AllCommands,
            RHI::AccessFlag::ShaderWrite,
            RHI::AccessFlag::ShaderRead,
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

    RHI::TextureHandle IBLBuilder::buildEnvCubemap(const std::string& hdrPath,uint32_t faceSize){

        return buildEnvCubemap(hdrPath, faceSize, true);  
    }

    RHI::TextureHandle IBLBuilder::buildEnvCubemap(const std::string& hdrPath,uint32_t faceSize,bool useComputeShader){
        
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
                RHI::AccessFlag::TransferWrite,
                RHI::AccessFlag::ShaderRead,
                range);
        }
        stbi_image_free(pixels);

        RHI::TextureHandle cubemap = useComputeShader? equirectToCubemapCS(equirectTex, faceSize): equirectToCubemap(equirectTex, faceSize);

        m_resMgr->destroy(equirectTex);
        return cubemap;
    }

} // namespace StarryEngine::Assets