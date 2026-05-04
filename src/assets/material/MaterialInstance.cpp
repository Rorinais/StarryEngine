#include "MaterialInstance.hpp"
#include "../../logging/Logger.hpp"
#include <cstring>

namespace StarryEngine::Assets {

    MaterialInstance::MaterialInstance(std::shared_ptr<MaterialTemplate> tmpl,
        RHI::DescriptorPoolHandle pool,
        RHI::ResourceManager* resMgr,
        RHI::DescriptorSetHandle globalSet)
        : m_template(tmpl), m_resMgr(resMgr), m_pool(pool) {
        // 获取布局映射
        m_layouts = tmpl->getLayouts();

        // 存储全局描述符集
        if (globalSet.isValid()) {
            m_sets[0] = globalSet;
        }

        // 从模板拷贝默认渲染状态
        if (m_template) {
            m_cullMode = m_template->getCullMode();
            m_frontFace = m_template->getFrontFace();
            m_depthTestEnable = m_template->isDepthTestEnable();
            m_depthWriteEnable = m_template->isDepthWriteEnable();
            m_depthCompareOp = m_template->getDethCompareOp();
            m_attachments = m_template->getAttachments();
            m_alphaBlend = m_template->isTransparent();
            m_isDeferred = m_template->isDeferred();
            m_debugName = m_template->getDebugName();
        }
    }

    RHI::DescriptorSetHandle MaterialInstance::getOrCreateSet(uint32_t setIndex) {
        auto it = m_sets.find(setIndex);
        if (it != m_sets.end()) {
            return it->second;
        }

        auto layoutIt = m_layouts.find(setIndex);
        if (layoutIt == m_layouts.end()) {
            return RHI::DescriptorSetHandle::Null();
        }

        RHI::DescriptorSetDesc desc;
        desc.descriptorPool = m_pool;
        desc.descriptorSetLayout = layoutIt->second;
        desc.setIndex = setIndex;
        desc.debugName = "MaterialSet_" + std::to_string(setIndex);
        auto set = m_resMgr->createDescriptorSet(desc);
        if (set.isValid()) {
            m_sets[setIndex] = set;
        }
        return set;
    }

    RHI::DescriptorSetHandle MaterialInstance::getSet(uint32_t setIndex) const {
        auto it = m_sets.find(setIndex);
        return it != m_sets.end() ? it->second : RHI::DescriptorSetHandle::Null();
    }

    void MaterialInstance::setUniform(uint32_t setIndex, uint32_t binding, const void* data, size_t size) {
        auto set = getOrCreateSet(setIndex);
        if (!set.isValid()) return;

        uint64_t key = ((uint64_t)setIndex << 32) | binding;
        auto it = m_buffers.find(key);
        if (it == m_buffers.end()) {
            RHI::BufferDesc bufDesc;
            bufDesc.size = size;
            bufDesc.type = RHI::BufferType::Uniform;
            bufDesc.memoryType = RHI::MemoryType::CPU_To_GPU;
            bufDesc.allowUpdate = true;
            bufDesc.persistentMapped = true;
            bufDesc.debugName = "MaterialUBO";

            auto buffer = m_resMgr->createBuffer(bufDesc);
            if (!buffer.isValid()) return;

            auto* bufferObj = m_resMgr->getBuffer(buffer);
            if (!bufferObj) return;
            void* mapped = bufferObj->map();

            m_buffers[key] = { buffer, mapped, size };

            auto* setObj = m_resMgr->getDescriptorSet(set);
            if (setObj) {
                setObj->writeBuffer(binding, 0, bufferObj, 0, size);
                setObj->update();
            }

            it = m_buffers.find(key);
        }

        if (it != m_buffers.end()) {
            std::memcpy(it->second.mappedData, data, size);
        }
    }

    void MaterialInstance::setTexture(uint32_t setIndex, uint32_t binding,
        RHI::TextureHandle texture, RHI::SamplerHandle sampler) {
        auto set = getOrCreateSet(setIndex);
        LOG_INFO("Material setTexture: set={}, binding={}, texture handle={}", setIndex, binding, texture.getIndex());

        if (!set.isValid()) return;

        auto* setObj = m_resMgr->getDescriptorSet(set);
        if (!setObj) return;
        auto* textureObj = m_resMgr->getTexture(texture);
        auto* samplerObj = m_resMgr->getSampler(sampler);
        if (!textureObj || !samplerObj) return;

        setObj->writeTexture(binding, 0, textureObj, samplerObj, RHI::ImageLayout::ShaderReadOnly);
        setObj->update();
    }

    void MaterialInstance::setInputAttachment(uint32_t setIndex, uint32_t binding,
        RHI::TextureHandle texture, RHI::ImageLayout layout) {
        LOG_INFO("Material setInputAttachment set={}, binding={}, texture={}, layout={}", setIndex, binding, texture.getIndex(), static_cast<int>(layout));

        auto set = getOrCreateSet(setIndex);
        if (!set.isValid()) return;

        auto* setObj = m_resMgr->getDescriptorSet(set);
        auto* textureObj = m_resMgr->getTexture(texture);
        if (!setObj || !textureObj) return;

        setObj->writeInputAttachment(binding, 0, textureObj, layout);
        setObj->update();
    }

    void MaterialInstance::setInstancingLayout(const InstancingLayout& layout) {
        m_instancingLayout = layout;
        m_hasCustomInstancingLayout = true;
    }

    const InstancingLayout* MaterialInstance::getInstancingLayout() const {
        if (m_hasCustomInstancingLayout) return &m_instancingLayout;
        return m_template ? m_template->getInstancingLayout() : nullptr;
    }

    void MaterialInstance::buildReflectionCache() {
        LOG_INFO("Building reflection cache for material...");
        if (m_reflectionCached) return;
        m_reflectionCached = true;

        auto* tmpl = dynamic_cast<DefaultMaterialTemplate*>(m_template.get());
        if (!tmpl) return;

        auto process = [&](const RHI::ShaderReflectionInfo& info) {
            for (const auto& res : info.resourceBindings) {
                if (res.set == 0) continue; // 跳过全局 set0，由引擎管理
                if (res.type == RHI::DescriptorType::UniformBuffer ||
                    res.type == RHI::DescriptorType::StorageBuffer) {
                    // 每个 UBO 创建一个参数块，并自动创建对应的 GPU 缓冲区
                    if (m_blocks.find(res.name) == m_blocks.end()) {
                        m_blocks.emplace(res.name, MaterialParameterBlock(res));
                        m_blockBindings[res.name] = { res.set, res.binding };

                        uint64_t key = ((uint64_t)res.set << 32) | res.binding;
                        if (m_buffers.find(key) == m_buffers.end()) {
                            size_t blockSize = m_blocks[res.name].size();
                            RHI::BufferDesc bufDesc;
                            bufDesc.size = blockSize;
                            bufDesc.type = RHI::BufferType::Uniform;
                            bufDesc.memoryType = RHI::MemoryType::CPU_To_GPU;
                            bufDesc.allowUpdate = true;
                            bufDesc.persistentMapped = true;
                            bufDesc.debugName = res.name;
                            auto buffer = m_resMgr->createBuffer(bufDesc);
                            if (!buffer.isValid()) continue;
                            auto* bufferObj = m_resMgr->getBuffer(buffer);
                            void* mapped = bufferObj->map();
                            m_buffers[key] = { buffer, mapped, blockSize };

                            // 绑定到描述符集
                            auto set = getOrCreateSet(res.set);
                            auto* setObj = m_resMgr->getDescriptorSet(set);
                            if (setObj) {
                                LOG_INFO("Created UBO buffer for '{}', size={}, set={}, binding={}",
                                    res.name, blockSize, res.set, res.binding);
                                setObj->writeBuffer(res.binding, 0, bufferObj, 0, blockSize);
                                setObj->update();
                            }
                        }
                    }
                }
                else if (res.type == RHI::DescriptorType::CombinedImageSampler) {
                    m_samplerBindings[res.name] = { res.set, res.binding };
                }
                else if (res.type == RHI::DescriptorType::InputAttachment) {
                    m_inputAttachmentBindings[res.name] = { res.set, res.binding };
                }
            }
            };

        process(tmpl->getVSReflection());
        process(tmpl->getFSReflection());
    }

    MaterialParameterBlock* MaterialInstance::getBlock(const std::string& blockName) {
        buildReflectionCache();
        auto it = m_blocks.find(blockName);
        return it != m_blocks.end() ? &it->second : nullptr;
    }

    void MaterialInstance::setTexture(const std::string& name, RHI::TextureHandle texture, RHI::SamplerHandle sampler) {
        buildReflectionCache();
        auto it = m_samplerBindings.find(name);
        if (it == m_samplerBindings.end()) {
            LOG_ERROR("Sampler '{}' not found", name);
            return;
        }
        setTexture(it->second.first, it->second.second, texture, sampler);
    }

    void MaterialInstance::addTextureDependency(const std::string& shaderVarName,
        const std::string& rgTextureName,
        ResourceDependencyType type) {
        buildReflectionCache();
        // 根据类型查找对应的绑定信息
        std::pair<uint32_t, uint32_t> bind = {};
        if (type == ResourceDependencyType::Sampler) {
            auto it = m_samplerBindings.find(shaderVarName);
            if (it == m_samplerBindings.end()) { LOG_ERROR("Sampler '{}' not found", shaderVarName); return; }
            bind = it->second;
        }
        else if (type == ResourceDependencyType::InputAttachment) {
            auto it = m_inputAttachmentBindings.find(shaderVarName);
            if (it == m_inputAttachmentBindings.end()) { LOG_ERROR("InputAttachment '{}' not found", shaderVarName); return; }
            bind = it->second;
        }
        m_textureDependencies[rgTextureName] = { bind.first, bind.second, type };
    }

    void MaterialInstance::applyAllDirtyBlocks() {
        for (auto& [name, block] : m_blocks) {
            if (!block.isDirty()) continue;
            auto& bind = m_blockBindings[name];
            uint64_t key = ((uint64_t)bind.first << 32) | bind.second;
            auto it = m_buffers.find(key);
            if (it != m_buffers.end()) {
                LOG_INFO("Uploading block '{}' ({} bytes) to set={}, binding={}",
                    name, block.size(), bind.first, bind.second);
                std::memcpy(it->second.mappedData, block.data(), block.size());
                block.clearDirty();
            }
            else {
                LOG_ERROR("Buffer not found for block '{}'", name);
            }
        }
    }

    std::shared_ptr<DefaultMaterialTemplate> MaterialInstance::createDefaultTemplate(std::shared_ptr<RHI::ResourceManager> resMgr, RHI::DescriptorSetLayoutHandle globalSetLayout) {
        std::unordered_map<uint32_t, RHI::DescriptorSetLayoutHandle> layouts;
        layouts[0] = globalSetLayout;

        std::vector<RHI::PushConstantRange> pushConstants = {
            {RHI::ShaderStage::Vertex, 0, sizeof(glm::mat4)}
        };
        return std::make_shared<DefaultMaterialTemplate>(resMgr, layouts, pushConstants);
    }

    std::shared_ptr<MaterialInstance> MaterialInstance::createDefault(
        std::shared_ptr<RHI::ResourceManager> resMgr,
        RHI::DescriptorSetLayoutHandle globalSetLayout,
        RHI::DescriptorPoolHandle pool,
        RHI::DescriptorSetHandle globalSet)
    {
        auto tmpl = createDefaultTemplate(resMgr, globalSetLayout);
        if (!tmpl->loadShaders("assets/shaders/mvp.vert", "assets/shaders/default.frag")) {
            LOG_ERROR("Failed to load default shaders, falling back to error material");
            return createError(resMgr, globalSetLayout, pool, globalSet);
        }

        auto material = std::make_shared<MaterialInstance>(tmpl, pool, resMgr.get(), globalSet);

        material->setSubpassTag("Forward_Opaque");
        return material;
    }

    std::shared_ptr<MaterialInstance> MaterialInstance::createError(
        std::shared_ptr<RHI::ResourceManager> resMgr,
        RHI::DescriptorSetLayoutHandle globalSetLayout,
        RHI::DescriptorPoolHandle pool,
        RHI::DescriptorSetHandle globalSet)
    {
        auto tmpl = createDefaultTemplate(resMgr, globalSetLayout);
        tmpl->loadShaders("assets/shaders/mvp.vert", "assets/shaders/error.frag");
        
        auto material = std::make_shared<MaterialInstance>(tmpl, pool, resMgr.get(), globalSet);

        material->setSubpassTag("Forward_Opaque");
        return material;
    }
}