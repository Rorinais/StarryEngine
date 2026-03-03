#include"Material.hpp"

namespace StarryEngine::RenderGraph {
    void Material::setVertexShader(const std::string& sourceCode, const std::string& debugName) {
        RHI::ShaderModuleDesc desc;
        desc.stage = RHI::ShaderStage::Vertex;
        desc.sourcecode = sourceCode;
        desc.includePaths = {};
        desc.debugName = debugName;
        mVertexShader = mResMgr->createShader(desc);
    }

    void Material::setFragmentShader(const std::string& sourceCode, const std::string& debugName) {
        RHI::ShaderModuleDesc desc;
        desc.stage = RHI::ShaderStage::Fragment;
        desc.sourcecode = sourceCode;
        desc.includePaths = {};
        desc.debugName = debugName;
        mFragmentShader = mResMgr->createShader(desc);
    }

    // --- 添加 Uniform Buffer（外部传入句柄）---
    void Material::addUniformBuffer(RHI::BufferHandle buffer, size_t size, uint32_t binding,
        RHI::ShaderStage stageFlags) {
        RHI::DescriptorBufferInfo info;
        info.buffer = buffer;
        info.offset = 0;
        info.range = size;
        addBinding(binding, RHI::DescriptorType::UniformBuffer, 1, stageFlags);
        mResources[binding] = DescriptorResourceInfo(info);
    }

    // --- 创建并添加 Uniform Buffer（材质自己创建）---
    RHI::BufferHandle Material::createAndAddUniformBuffer(size_t size, uint32_t binding,
        const std::string& debugName,
        RHI::ShaderStage stageFlags) {
        RHI::BufferDesc desc;
        desc.size = size;
        desc.type = RHI::BufferType::Uniform;
        desc.memoryType = RHI::MemoryType::CPU_To_GPU;
        desc.allowUpdate = true;
        desc.persistentMapped = true;
        desc.debugName = debugName;

        RHI::BufferHandle buffer = mResMgr->createBuffer(desc);
        if (buffer.isValid()) {
            addUniformBuffer(buffer, size, binding, stageFlags);
        }
        return buffer;
    }

    // --- 添加纹理（自动创建采样器）---
    RHI::TextureHandle Material::addTexture(const std::string& filename,
        RHI::Format format,
        const std::string& debugName,
        uint32_t binding,
        RHI::ShaderStage stageFlags) {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(filename.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        if (!pixels) {
            std::cerr << "[Material] Failed to load texture: " << filename << std::endl;
            return RHI::TextureHandle::Null();
        }

        // 创建纹理
        RHI::TextureDesc texDesc;
        texDesc.extent = { (uint32_t)texWidth, (uint32_t)texHeight, 1 };
        texDesc.format = format;
        texDesc.type = RHI::TextureType::Texture2D;
        texDesc.mipLevels = 1;
        texDesc.arrayLayers = 1;
        texDesc.sampleCount = 1;
        texDesc.allowRenderTarget = false;
        texDesc.allowDepthStencil = false;
        texDesc.allowUnorderedAccess = false;
        texDesc.debugName = debugName;

        RHI::TextureHandle texHandle = mResMgr->createTexture(texDesc);
        if (!texHandle.isValid()) {
            stbi_image_free(pixels);
            return RHI::TextureHandle::Null();
        }

        auto* texture = mResMgr->getTexture(texHandle);
        texture->update(pixels, texWidth * texHeight * 4,
            { RHI::ImageAspect::Color, 0, 1, 0, 1 });
        // 在 Material::addTexture 中，纹理上传后立即转换布局
        texture->transitionLayout(
            RHI::ImageLayout::ShaderReadOnly,              // 目标布局
            RHI::PipelineStage::Transfer,                  // 源阶段（上传操作）
            RHI::PipelineStage::FragmentShader,             // 目标阶段（片段着色器）
            static_cast<RHI::AccessFlags>(RHI::AccessFlag::TransferWrite), // 源访问
            static_cast<RHI::AccessFlags>(RHI::AccessFlag::ShaderRead),     // 目标访问
            { RHI::ImageAspect::Color, 0, 1, 0, 1 }        // 子资源范围
        );

        // 创建采样器
        RHI::SamplerDesc samplerDesc;
        samplerDesc.magFilter = RHI::SamplerFilter::Linear;
        samplerDesc.minFilter = RHI::SamplerFilter::Linear;
        samplerDesc.addressU = RHI::SamplerAddressMode::Repeat;
        samplerDesc.addressV = RHI::SamplerAddressMode::Repeat;
        samplerDesc.addressW = RHI::SamplerAddressMode::Repeat;
        samplerDesc.maxAnisotropy = 1.0f;
        samplerDesc.debugName = debugName + "_Sampler";

        RHI::SamplerHandle samplerHandle = mResMgr->createSampler(samplerDesc);

        // 存储纹理-采样器对，并添加资源信息
        RHI::DescriptorImageInfo imageInfo;
        imageInfo.texture = texHandle;
        imageInfo.sampler = samplerHandle;
        imageInfo.imageLayout = RHI::ImageLayout::ShaderReadOnly;

        addBinding(binding, RHI::DescriptorType::CombinedImageSampler, 1, stageFlags);
        mResources[binding] = DescriptorResourceInfo(imageInfo);

        stbi_image_free(pixels);
        return texHandle;
    }

    void Material::addInputAttachment(RHI::TextureHandle texture, uint32_t binding,
        RHI::ImageLayout layout, RHI::ShaderStage stageFlags) {
        RHI::DescriptorImageInfo info;
        info.texture = texture;
        info.sampler = RHI::SamplerHandle::Null();
        info.imageLayout = layout;
        addBinding(binding, RHI::DescriptorType::InputAttachment, 1, stageFlags);
        DescriptorResourceInfo resInfo;
        resInfo.data = info;          // 将 info 存入 variant
        resInfo.type = RHI::DescriptorType::InputAttachment; // 手动设置类型
        mResources[binding] = resInfo;
    }

    // --- 创建描述符集布局（基于已添加的 binding）---
    bool Material::createDescriptorSetLayout() {
        if (mBindings.empty()) {
            std::cerr << "[Material] No bindings defined, cannot create layout." << std::endl;
            return false;
        }

        std::vector<RHI::DescriptorSetLayoutBinding> bindings;
        for (const auto& [binding, info] : mBindings) {
            RHI::DescriptorSetLayoutBinding b{};
            b.binding = binding;
            b.type = info.type;
            b.count = info.count;
            b.stageFlags = info.stageFlags;
            b.immutableSamplers = false;
            bindings.push_back(b);
        }

        RHI::DescriptorSetLayoutDesc desc;
        desc.bindings = bindings;
        desc.debugName = "MaterialLayout";

        mDescriptorSetLayout = mResMgr->createDescriptorSetLayout(desc);
        return mDescriptorSetLayout.isValid();
    }

    // --- 从外部池分配描述符集 ---
    bool Material::allocateDescriptorSet(RHI::DescriptorPoolHandle pool,
        RHI::PipelineLayoutHandle pipelineLayout,
        uint32_t setIndex) {
        if (!mDescriptorSetLayout.isValid()) {
            std::cerr << "[Material] Descriptor set layout not created." << std::endl;
            return false;
        }

        RHI::DescriptorSetDesc desc;
        desc.descriptorPool = pool;
        desc.pipelineLayout = pipelineLayout;
        desc.setIndex = setIndex;
        desc.debugName = "MaterialSet";

        mDescriptorSet = mResMgr->createDescriptorSet(desc);
        if (mDescriptorSet.isValid()) {
            mPipelineLayout = pipelineLayout;
            mSetIndex = setIndex;
        }
        return mDescriptorSet.isValid();
    }

    // --- 更新描述符集（遍历所有资源）---
    void Material::updateDescriptorSet() {
        if (!mDescriptorSet.isValid()) return;
        auto* set = mResMgr->getDescriptorSet(mDescriptorSet);
        if (!set) return;

        for (const auto& [binding, resource] : mResources) {
            if (std::holds_alternative<RHI::DescriptorBufferInfo>(resource.data)) {
                const auto& bufferInfo = std::get<RHI::DescriptorBufferInfo>(resource.data);
                auto* buffer = mResMgr->getBuffer(bufferInfo.buffer);
                if (buffer) set->writeBuffer(binding, 0, buffer, bufferInfo.offset, bufferInfo.range);
            }
            else if (std::holds_alternative<RHI::DescriptorImageInfo>(resource.data)) {
                const auto& imageInfo = std::get<RHI::DescriptorImageInfo>(resource.data);
                auto* texture = mResMgr->getTexture(imageInfo.texture);
                std::cout << "[Material] Updating descriptor set, binding " << binding
                    << " texture handle: " << imageInfo.texture.toString() << std::endl;
                if (resource.type == RHI::DescriptorType::InputAttachment) {
                    // 输入附件写入
                    if (texture) set->writeInputAttachment(binding, 0, texture, imageInfo.imageLayout);
                }
                else {
                    // 假设是 CombinedImageSampler
                    auto* sampler = mResMgr->getSampler(imageInfo.sampler);
                    if (texture && sampler) set->writeTexture(binding, 0, texture, sampler, imageInfo.imageLayout);
                }
            }
        }

        set->update();
    }

    void Material::updateInputAttachment(uint32_t binding, RHI::TextureHandle texture, RHI::ImageLayout layout) {
        if (!mDescriptorSet.isValid()) return;
        auto* set = mResMgr->getDescriptorSet(mDescriptorSet);
        if (!set) return;
        auto* tex = mResMgr->getTexture(texture);
        if (tex) {
            set->writeInputAttachment(binding, 0, tex, layout);
            set->update();
        }
    }
}