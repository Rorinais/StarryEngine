#include <assets/material/MaterialInstance.hpp>
#include <logging/Logger.hpp>
#include <cstring>

namespace StarryEngine::Assets {

    MaterialInstance::MaterialInstance(std::shared_ptr<MaterialTemplate> tmpl,
        RHI::DescriptorPoolHandle pool,
        RHI::ResourceManager* resMgr,
        RHI::DescriptorSetHandle globalSet)
        : m_template(tmpl), m_resMgr(resMgr) {
        
        RHI::DescriptorPoolDesc poolDesc;
        // 容量翻倍：per-slot 描述符集（每 set 两槽 = 2 倍集数；集内 UBO/SSBO 描述符也翻倍）。
        poolDesc.maxSets = 16 * RHI::kMaxFramesInFlight;
        poolDesc.poolSizes = {
            { RHI::DescriptorType::UniformBuffer,         8  * RHI::kMaxFramesInFlight },
            { RHI::DescriptorType::StorageBuffer,         4  * RHI::kMaxFramesInFlight },
            { RHI::DescriptorType::CombinedImageSampler, 16 * RHI::kMaxFramesInFlight },
            { RHI::DescriptorType::InputAttachment,       8  * RHI::kMaxFramesInFlight }
        };
        poolDesc.freeDescriptorSet = true;
        poolDesc.debugName = "MaterialPool";
        m_pool = m_resMgr->createDescriptorPool(poolDesc);
        m_layouts = tmpl->getLayouts();

        if (globalSet.isValid()) {
            // 集 0 是废码（渲染期被 SceneAnalyzer 的 per-slot global 覆盖），两槽同句柄保持兼容。
            m_sets[0] = std::vector<RHI::DescriptorSetHandle>(RHI::kMaxFramesInFlight, globalSet);
        }

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

    MaterialInstance::~MaterialInstance() {
        if (m_pool.isValid())
            m_resMgr->destroy(m_pool);
    }

    RHI::DescriptorSetHandle MaterialInstance::getOrCreateSet(uint32_t setIndex) {
        // 无槽版本：建满两槽（首次调用），返回槽 0 的集（兼容"建好后批量绑定"的调用方）。
        RHI::DescriptorSetHandle first = RHI::DescriptorSetHandle::Null();
        for (uint32_t s = 0; s < RHI::kMaxFramesInFlight; ++s) {
            auto h = getOrCreateSet(setIndex, s);
            if (s == 0) first = h;
        }
        return first;
    }

    RHI::DescriptorSetHandle MaterialInstance::getOrCreateSet(uint32_t setIndex, uint32_t slot) {
        auto it = m_sets.find(setIndex);
        if (it == m_sets.end()) {
            it = m_sets.emplace(setIndex,
                std::vector<RHI::DescriptorSetHandle>(RHI::kMaxFramesInFlight, RHI::DescriptorSetHandle::Null())).first;
        }
        else if (it->second.size() < RHI::kMaxFramesInFlight) {
            it->second.resize(RHI::kMaxFramesInFlight, RHI::DescriptorSetHandle::Null());
        }
        if (it->second[slot].isValid()) return it->second[slot];

        auto layoutIt = m_layouts.find(setIndex);
        if (layoutIt == m_layouts.end()) {
            return RHI::DescriptorSetHandle::Null();
        }

        RHI::DescriptorSetDesc desc;
        desc.descriptorPool = m_pool;
        desc.descriptorSetLayout = layoutIt->second;
        desc.setIndex = setIndex;
        desc.debugName = "MaterialSet_" + std::to_string(setIndex) + "_slot" + std::to_string(slot);
        auto set = m_resMgr->createDescriptorSet(desc);
        if (set.isValid()) {
            it->second[slot] = set;
        }
        return set;
    }

    RHI::DescriptorSetHandle MaterialInstance::getSet(uint32_t setIndex, uint32_t slot) const {
        auto it = m_sets.find(setIndex);
        if (it == m_sets.end() || slot >= it->second.size()) return RHI::DescriptorSetHandle::Null();
        return it->second[slot];
    }

    void MaterialInstance::setUniform(uint32_t setIndex, uint32_t binding, const void* data, size_t size, uint32_t slot) {
        // 根据 descriptor 布局自动区分 UBO / SSBO（SSBO 用于超 UBO 限制的大块数据，如骨骼矩阵）
        setBufferData(setIndex, binding, data, size, resolveBindingDescriptorType(setIndex, binding), slot);
    }

    void MaterialInstance::setStorageBuffer(uint32_t setIndex, uint32_t binding, const void* data, size_t size, uint32_t slot) {
        setBufferData(setIndex, binding, data, size, RHI::DescriptorType::StorageBuffer, slot);
    }

    void MaterialInstance::setBufferData(uint32_t setIndex, uint32_t binding,
        const void* data, size_t size, RHI::DescriptorType descriptorType, uint32_t slot) {
        uint32_t targetSlot = resolveSlot(slot);

        RHI::BufferType bufferType = (descriptorType == RHI::DescriptorType::StorageBuffer ||
            descriptorType == RHI::DescriptorType::StorageBufferDynamic)
            ? RHI::BufferType::Storage : RHI::BufferType::Uniform;

        uint64_t key = ((uint64_t)setIndex << 32) | binding;
        auto it = m_buffers.find(key);

        // 已有 buffer 但尺寸不足（如反射预建的 0 尺寸 SSBO），销毁全部槽位后按新尺寸重建
        if (it != m_buffers.end()) {
            bool growNeeded = false;
            for (uint32_t s = 0; s < RHI::kMaxFramesInFlight; ++s) {
                if (it->second.perSlot[s].buffer.isValid() && it->second.perSlot[s].size < size) {
                    growNeeded = true;
                    break;
                }
            }
            if (growNeeded) {
                for (uint32_t s = 0; s < RHI::kMaxFramesInFlight; ++s) {
                    if (it->second.perSlot[s].buffer.isValid()) {
                        auto old = it->second.perSlot[s];
                        m_resMgr->scheduleDestroy([old, resMgr = m_resMgr]() { resMgr->destroy(old.buffer); }, 2);
                    }
                }
                m_buffers.erase(it);
                it = m_buffers.end();
            }
        }

        if (it == m_buffers.end()) {
            // 首调：为所有槽位建 buffer + 写各自描述符 + 灌入初始数据（writtenOnce=true），
            // 否则另一槽第一次上屏是空/旧数据。
            SlotBuffers sb;
            for (uint32_t s = 0; s < RHI::kMaxFramesInFlight; ++s) {
                RHI::BufferDesc bufDesc;
                bufDesc.size = size;
                bufDesc.type = bufferType;
                bufDesc.memoryType = RHI::MemoryType::CPU_To_GPU;
                bufDesc.allowUpdate = true;
                bufDesc.persistentMapped = true;
                bufDesc.debugName = (bufferType == RHI::BufferType::Storage) ? "MaterialSSBO" : "MaterialUBO";

                auto buffer = m_resMgr->createBuffer(bufDesc);
                if (!buffer.isValid()) continue;

                auto* bufferObj = m_resMgr->getBuffer(buffer);
                if (!bufferObj) continue;
                void* mapped = bufferObj->map();

                sb.perSlot[s] = { buffer, mapped, size, bufferType };

                auto set = getOrCreateSet(setIndex, s);
                if (!set.isValid()) continue;
                auto* setObj = m_resMgr->getDescriptorSet(set);
                if (setObj) {
                    setObj->writeBuffer(binding, 0, bufferObj, 0, size);
                    setObj->update();
                }
            }
            // 灌满两槽（第一帧无论槽位都要有数据）
            for (uint32_t s = 0; s < RHI::kMaxFramesInFlight; ++s) {
                if (sb.perSlot[s].buffer.isValid() && sb.perSlot[s].mappedData)
                    std::memcpy(sb.perSlot[s].mappedData, data, size);
            }
            sb.writtenOnce = true;
            m_buffers[key] = sb;
            return;
        }

        // 稳态：只写目标槽（该槽位对应当前帧，别的槽位可能在飞 GPU 读）
        if (targetSlot < RHI::kMaxFramesInFlight &&
            it->second.perSlot[targetSlot].buffer.isValid() &&
            it->second.perSlot[targetSlot].mappedData) {
            std::memcpy(it->second.perSlot[targetSlot].mappedData, data, size);
        }
    }

    RHI::DescriptorType MaterialInstance::resolveBindingDescriptorType(uint32_t setIndex, uint32_t binding) const {
        auto layoutIt = m_layouts.find(setIndex);
        if (layoutIt == m_layouts.end()) return RHI::DescriptorType::UniformBuffer;

        auto* layout = m_resMgr->getDescriptorSetLayout(layoutIt->second);
        if (!layout) return RHI::DescriptorType::UniformBuffer;

        for (const auto& b : layout->getBindings()) {
            if (b.binding == binding) {
                return b.type;
            }
        }
        return RHI::DescriptorType::UniformBuffer;
    }

    void MaterialInstance::setTexture(uint32_t setIndex, uint32_t binding,
        RHI::TextureHandle texture, RHI::SamplerHandle sampler) {
        LOG_INFO("Material setTexture: set={}, binding={}, texture handle={}", setIndex, binding, texture.getIndex());

        auto* textureObj = m_resMgr->getTexture(texture);
        auto* samplerObj = m_resMgr->getSampler(sampler);
        if (!textureObj || !samplerObj) return;

        // 纹理绑定写两槽：两槽的集都要引用同一纹理（纹理不 per-frame 变）
        for (uint32_t s = 0; s < RHI::kMaxFramesInFlight; ++s) {
            auto set = getOrCreateSet(setIndex, s);
            if (!set.isValid()) continue;
            auto* setObj = m_resMgr->getDescriptorSet(set);
            if (!setObj) continue;
            setObj->writeTexture(binding, 0, textureObj, samplerObj, RHI::ImageLayout::ShaderReadOnly);
            setObj->update();
        }
    }

    void MaterialInstance::setInputAttachment(uint32_t setIndex, uint32_t binding,
        RHI::TextureHandle texture, RHI::ImageLayout layout) {
        LOG_INFO("Material setInputAttachment set={}, binding={}, texture={}, layout={}", setIndex, binding, texture.getIndex(), static_cast<int>(layout));

        auto* textureObj = m_resMgr->getTexture(texture);
        if (!textureObj) return;

        for (uint32_t s = 0; s < RHI::kMaxFramesInFlight; ++s) {
            auto set = getOrCreateSet(setIndex, s);
            if (!set.isValid()) continue;
            auto* setObj = m_resMgr->getDescriptorSet(set);
            if (!setObj) continue;
            setObj->writeInputAttachment(binding, 0, textureObj, layout);
            setObj->update();
        }
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
        if (m_reflectionCached) return;
        m_reflectionCached = true;

        auto* tmpl = dynamic_cast<DefaultMaterialTemplate*>(m_template.get());
        if (!tmpl) return;

        auto getEffectiveBinding = [&](const RHI::ResourceBinding& reflected) -> const RHI::ResourceBinding& {
            auto manualIt = m_blockLayouts.find(reflected.name);
            if (manualIt != m_blockLayouts.end()) {
                LOG_INFO("Block '{}' found in shader reflection ({} members), "
                    "ignoring incomplete manual registration",
                    reflected.name, reflected.members.size());
            }
            return reflected; 
            };

        auto createBlockIfNeeded = [&](const RHI::ResourceBinding& binding) {
            if (binding.set == 0) return;

            bool blockExisted = (m_blocks.find(binding.name) != m_blocks.end());

            if (!blockExisted) {
                m_blocks.emplace(binding.name, MaterialParameterBlock(binding));
                m_blockBindings[binding.name] = { binding.set, binding.binding };
                LOG_INFO("Created block '{}' (set={}, binding={}, size={})",
                    binding.name, binding.set, binding.binding,
                    m_blocks[binding.name].size());
            }

            uint64_t key = ((uint64_t)binding.set << 32) | binding.binding;
            if (m_buffers.find(key) == m_buffers.end()) {
                size_t blockSize = m_blocks[binding.name].size();
                // 运行时数组的 SSBO（如 mat4 bones[]）反射成员为空 → blockSize == 0，
                // 不预建 buffer，由 setStorageBuffer/setUniform 显式创建真实尺寸。
                if (blockSize > 0) {
                    ensureGPUBufferForBlock(binding.name, binding.set, binding.binding,
                        blockSize, binding.type);
                }
                else {
                    LOG_INFO("Block '{}' has no fixed members (runtime-array SSBO?), "
                        "deferring buffer creation to explicit setStorageBuffer/setUniform",
                        binding.name);
                }
            }
            };

        auto process = [&](const RHI::ShaderReflectionInfo& info) {
            for (const auto& res : info.resourceBindings) {
                if (res.set == 0) continue;

                if (res.type == RHI::DescriptorType::UniformBuffer ||
                    res.type == RHI::DescriptorType::StorageBuffer) {

                    const auto& effective = getEffectiveBinding(res);
                    createBlockIfNeeded(effective);
                }
                else if (res.type == RHI::DescriptorType::CombinedImageSampler) {
                    if (m_samplerBindings.find(res.name) == m_samplerBindings.end()) {
                        m_samplerBindings[res.name] = { res.set, res.binding };
                    }
                }
                else if (res.type == RHI::DescriptorType::InputAttachment) {
                    if (m_inputAttachmentBindings.find(res.name) == m_inputAttachmentBindings.end()) {
                        m_inputAttachmentBindings[res.name] = { res.set, res.binding };
                    }
                }
            }
            };

        process(tmpl->getVSReflection());
        process(tmpl->getFSReflection());

        for (const auto& [name, layout] : m_blockLayouts) {
            if (m_blocks.find(name) == m_blocks.end()) {
                m_blocks.emplace(name, MaterialParameterBlock(layout));
                m_blockBindings[name] = { layout.set, layout.binding };
                LOG_INFO("Block '{}' registered but not in current shader, data preserved", name);
            }
        }
    }

    MaterialParameterBlock* MaterialInstance::getBlock(const std::string& blockName) {
        buildReflectionCache();

        auto it = m_blocks.find(blockName);
        if (it != m_blocks.end()) {
            return &it->second;
        }

        auto layoutIt = m_blockLayouts.find(blockName);
        if (layoutIt == m_blockLayouts.end()) {
            LOG_WARN("Block '{}' never registered and not in shader reflection", blockName);
            return nullptr;
        }

        const auto& res = layoutIt->second;
        uint32_t setIdx = res.set;
        uint32_t binding = res.binding;

        auto [insertedIt, _] = m_blocks.emplace(blockName, MaterialParameterBlock(res));
        m_blockBindings[blockName] = { setIdx, binding };

        LOG_INFO("Block '{}' created from manual layout (set={}, binding={}), no GPU buffer yet",
            blockName, setIdx, binding);

        return &insertedIt->second;
    }

    void MaterialInstance::registerBlockLayout(const std::string& blockName, const RHI::ResourceBinding& binding) {
        m_blockLayouts[blockName] = binding;
    }

    void MaterialInstance::setTexture(const std::string& name,
        RHI::TextureHandle texture,
        RHI::SamplerHandle sampler) {
        buildReflectionCache();

        auto it = m_samplerBindings.find(name);
        if (it == m_samplerBindings.end()) {
            LOG_WARN("Sampler '{}' not found in current shader, caching for future use", name);
            m_cachedTextures[name] = { UINT32_MAX, UINT32_MAX, texture, sampler };
            return;
        }

        uint32_t set = it->second.first;
        uint32_t binding = it->second.second;

        m_cachedTextures[name] = { set, binding, texture, sampler };

        setTexture(set, binding, texture, sampler);
    }

    void MaterialInstance::addTextureDependency(const std::string& shaderVarName,
        const std::string& rgTextureName,
        ResourceDependencyType type) {
        buildReflectionCache();
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

    void MaterialInstance::applyAllDirtyBlocks(uint32_t slot) {
        uint32_t targetSlot = resolveSlot(slot);
        for (auto& [name, block] : m_blocks) {
            if (!block.isDirty()) continue;

            auto bindIt = m_blockBindings.find(name);
            if (bindIt == m_blockBindings.end()) continue;

            uint32_t setIdx = bindIt->second.first;
            uint32_t binding = bindIt->second.second;

            auto layoutIt = m_layouts.find(setIdx);
            if (layoutIt == m_layouts.end()) {
                block.clearDirty();
                continue;
            }

            uint64_t key = ((uint64_t)setIdx << 32) | binding;
            auto bufIt = m_buffers.find(key);
            if (bufIt != m_buffers.end()) {
                auto& sb = bufIt->second;
                if (!sb.writtenOnce) {
                    // 首写：灌满两槽（另一槽第一次上屏也得有数据）
                    for (uint32_t s = 0; s < RHI::kMaxFramesInFlight; ++s) {
                        if (sb.perSlot[s].buffer.isValid() && sb.perSlot[s].mappedData)
                            std::memcpy(sb.perSlot[s].mappedData, block.data(), block.size());
                    }
                    sb.writtenOnce = true;
                }
                else if (targetSlot < RHI::kMaxFramesInFlight &&
                    sb.perSlot[targetSlot].buffer.isValid() && sb.perSlot[targetSlot].mappedData) {
                    std::memcpy(sb.perSlot[targetSlot].mappedData, block.data(), block.size());
                }
                block.clearDirty();
                LOG_INFO("Uploaded block '{}' ({} bytes)", name, block.size());
            }
            else {
                LOG_INFO("Binding not present for block '{}', deferring upload (data preserved)", name);
            }
        }

        for (auto& [name, block] : m_blocks) {
            m_savedBlockData[name] = std::vector<uint8_t>(block.data(), block.data() + block.size());
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

    void MaterialInstance::recreateDescriptorSets() {
        backupBlockValues();

        m_resMgr->destroy(m_pool);

        RHI::DescriptorPoolDesc poolDesc;
        poolDesc.maxSets = 16 * RHI::kMaxFramesInFlight;
        poolDesc.poolSizes = {
            { RHI::DescriptorType::UniformBuffer,         8  * RHI::kMaxFramesInFlight },
            { RHI::DescriptorType::StorageBuffer,         4  * RHI::kMaxFramesInFlight },
            { RHI::DescriptorType::CombinedImageSampler, 16 * RHI::kMaxFramesInFlight },
            { RHI::DescriptorType::InputAttachment,       8  * RHI::kMaxFramesInFlight }
        };
        poolDesc.freeDescriptorSet = true;
        poolDesc.debugName = "MaterialPool";
        m_pool = m_resMgr->createDescriptorPool(poolDesc);

        // 保留集 0（构造注入的 globalSet，废码，两槽同句柄）
        std::vector<RHI::DescriptorSetHandle> globalSets;
        if (auto it = m_sets.find(0); it != m_sets.end() && !it->second.empty())
            globalSets = it->second;
        m_sets.clear();
        if (!globalSets.empty()) m_sets[0] = globalSets;
        else m_sets[0] = std::vector<RHI::DescriptorSetHandle>(RHI::kMaxFramesInFlight, RHI::DescriptorSetHandle::Null());

        m_layouts = m_template->getLayouts();

        invalidateReflectionCache();
        buildReflectionCache();
        restoreBlockValues();
        restoreCachedBindings();
        applyAllDirtyBlocks();
    }

    void MaterialInstance::invalidateReflectionCache() {
        for (auto& [key, sb] : m_buffers) {
            for (uint32_t s = 0; s < RHI::kMaxFramesInFlight; ++s) {
                if (sb.perSlot[s].buffer.isValid()) {
                    m_resMgr->scheduleDestroy([buf = sb.perSlot[s].buffer, resMgr = m_resMgr]() {
                        resMgr->destroy(buf);
                        }, 2);
                }
            }
        }
        m_buffers.clear();
        m_blocks.clear();
        m_blockBindings.clear();
        m_samplerBindings.clear();
        m_inputAttachmentBindings.clear();
        m_reflectionCached = false;
    }

    void MaterialInstance::backupBlockValues() {
        for (auto& [name, block] : m_blocks) {
            m_savedBlockData[name] = std::vector<uint8_t>(block.data(), block.data() + block.size());
        }
    }

    void MaterialInstance::restoreBlockValues() {
        for (auto& [name, block] : m_blocks) {
            auto savedIt = m_savedBlockData.find(name);
            if (savedIt == m_savedBlockData.end()) continue;

            size_t savedSize = savedIt->second.size();
            size_t blockSize = block.size();

            if (savedSize == blockSize) {
                std::memcpy(block.data(), savedIt->second.data(), blockSize);
                block.markDirty();
                LOG_INFO("Restored block '{}', size={} (exact match)", name, blockSize);
            }
            else {
                size_t copySize = std::min(savedSize, blockSize);
                std::memcpy(block.data(), savedIt->second.data(), copySize);

                if (blockSize > copySize) {
                    std::memset(block.data() + copySize, 0, blockSize - copySize);
                }

                block.markDirty();
                LOG_WARN("Block '{}' size changed (saved={}, current={}), copied {} bytes, remainder zeroed",
                    name, savedSize, blockSize, copySize);
            }
        }
    }

    void MaterialInstance::restoreCachedBindings() {
        for (auto& [name, cached] : m_cachedTextures) {
            auto it = m_samplerBindings.find(name);
            if (it != m_samplerBindings.end()) {
                cached.set = it->second.first;
                cached.binding = it->second.second;
                setTexture(cached.set, cached.binding, cached.texture, cached.sampler);
                LOG_INFO("Restored sampler '{}' to set={}, binding={}", name, cached.set, cached.binding);
            }
            else {
                LOG_WARN("Sampler '{}' not in current shader, keeping cached data", name);
            }
        }
    }

    void MaterialInstance::ensureGPUBufferForBlock(
        const std::string& blockName,
        uint32_t setIdx, uint32_t binding,
        size_t blockSize, RHI::DescriptorType descriptorType) {

        uint64_t key = ((uint64_t)setIdx << 32) | binding;

        if (m_buffers.find(key) != m_buffers.end()) return;

        RHI::BufferType bufferType = (descriptorType == RHI::DescriptorType::StorageBuffer ||
            descriptorType == RHI::DescriptorType::StorageBufferDynamic)
            ? RHI::BufferType::Storage : RHI::BufferType::Uniform;

        // 反射预建：为所有槽位建 buffer + 描述符，但不 memcpy（数据由 applyAllDirtyBlocks
        // 首写时灌满两槽，writtenOnce 保持 false）。
        SlotBuffers sb;
        for (uint32_t s = 0; s < RHI::kMaxFramesInFlight; ++s) {
            RHI::BufferDesc bufDesc;
            bufDesc.size = blockSize;
            bufDesc.type = bufferType;
            bufDesc.memoryType = RHI::MemoryType::CPU_To_GPU;
            bufDesc.allowUpdate = true;
            bufDesc.persistentMapped = true;
            bufDesc.debugName = blockName;

            auto buffer = m_resMgr->createBuffer(bufDesc);
            if (!buffer.isValid()) {
                LOG_ERROR("Failed to create GPU buffer for block '{}' slot {}", blockName, s);
                continue;
            }

            auto* bufferObj = m_resMgr->getBuffer(buffer);
            if (!bufferObj) continue;

            void* mapped = bufferObj->map();
            sb.perSlot[s] = { buffer, mapped, blockSize, bufferType };

            auto set = getOrCreateSet(setIdx, s);
            auto* setObj = m_resMgr->getDescriptorSet(set);
            if (setObj) {
                setObj->writeBuffer(binding, 0, bufferObj, 0, blockSize);
                setObj->update();
                LOG_INFO("Created GPU buffer for '{}', size={}, set={}, binding={}, slot={}",
                    blockName, blockSize, setIdx, binding, s);
            }
            else {
                LOG_ERROR("Failed to get descriptor set {} for block '{}'", setIdx, blockName);
            }
        }
        m_buffers[key] = sb;   // writtenOnce 保持 false
    }
}