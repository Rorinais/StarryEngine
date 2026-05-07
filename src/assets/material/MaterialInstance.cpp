#include "MaterialInstance.hpp"
#include "../../logging/Logger.hpp"
#include <cstring>

namespace StarryEngine::Assets {

    MaterialInstance::MaterialInstance(std::shared_ptr<MaterialTemplate> tmpl,
        RHI::DescriptorPoolHandle pool,
        RHI::ResourceManager* resMgr,
        RHI::DescriptorSetHandle globalSet)
        : m_template(tmpl), m_resMgr(resMgr) {
        
        RHI::DescriptorPoolDesc poolDesc;
        poolDesc.maxSets = 8;   // 一个材质通常有 2~3 个 Set，8 足够
        poolDesc.poolSizes = {
            { RHI::DescriptorType::UniformBuffer, 4 },
            { RHI::DescriptorType::CombinedImageSampler, 8 },
            { RHI::DescriptorType::InputAttachment, 4 }
        };
        poolDesc.freeDescriptorSet = true;
        poolDesc.debugName = "MaterialPool";
        m_pool = m_resMgr->createDescriptorPool(poolDesc);
        // ───────────────────────────

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

    MaterialInstance::~MaterialInstance() {
        if (m_pool.isValid())
            m_resMgr->destroy(m_pool);
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
        if (m_reflectionCached) return;
        m_reflectionCached = true;

        auto* tmpl = dynamic_cast<DefaultMaterialTemplate*>(m_template.get());
        if (!tmpl) return;

        // ── 辅助：从反射获取最终使用的布局（反射优先）──
        auto getEffectiveBinding = [&](const RHI::ResourceBinding& reflected) -> const RHI::ResourceBinding& {
            auto manualIt = m_blockLayouts.find(reflected.name);
            if (manualIt != m_blockLayouts.end()) {
                LOG_INFO("Block '{}' found in shader reflection ({} members), "
                    "ignoring incomplete manual registration",
                    reflected.name, reflected.members.size());
            }
            return reflected;  // ← 反射优先
            };

        // ── 辅助：创建 block 和 GPU buffer ──
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
                ensureGPUBufferForBlock(binding.name, binding.set, binding.binding, blockSize);
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

        // ── 补充：手动注册过、但当前 shader 中不存在的 block ──
        for (const auto& [name, layout] : m_blockLayouts) {
            if (m_blocks.find(name) == m_blocks.end()) {
                m_blocks.emplace(name, MaterialParameterBlock(layout));
                m_blockBindings[name] = { layout.set, layout.binding };
                // 不创建 GPU buffer，当前 pipeline layout 不需要它
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

        // ── Fallback：手动注册的 block（当前 shader 中不存在）──
        auto layoutIt = m_blockLayouts.find(blockName);
        if (layoutIt == m_blockLayouts.end()) {
            LOG_WARN("Block '{}' never registered and not in shader reflection", blockName);
            return nullptr;
        }

        const auto& res = layoutIt->second;
        uint32_t setIdx = res.set;
        uint32_t binding = res.binding;

        // 创建 MaterialParameterBlock
        auto [insertedIt, _] = m_blocks.emplace(blockName, MaterialParameterBlock(res));
        m_blockBindings[blockName] = { setIdx, binding };

        // ⚠️ 不创建 GPU buffer（当前 shader 中该 binding 不存在）
        //   数据保留在 block 中，等待热重载后 binding 出现时再创建 buffer

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
            // Shader 中暂时没有这个采样器 → 只缓存，不更新描述符集
            LOG_WARN("Sampler '{}' not found in current shader, caching for future use", name);
            m_cachedTextures[name] = { UINT32_MAX, UINT32_MAX, texture, sampler };
            return;
        }

        uint32_t set = it->second.first;
        uint32_t binding = it->second.second;

        // 记录完整绑定信息
        m_cachedTextures[name] = { set, binding, texture, sampler };

        // 立即写入描述符集
        setTexture(set, binding, texture, sampler);
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

            auto bindIt = m_blockBindings.find(name);
            if (bindIt == m_blockBindings.end()) continue;

            uint32_t setIdx = bindIt->second.first;
            uint32_t binding = bindIt->second.second;

            auto layoutIt = m_layouts.find(setIdx);
            if (layoutIt == m_layouts.end()) {
                block.clearDirty();  // set 不存在，保留数据但不再重试
                continue;
            }

            uint64_t key = ((uint64_t)setIdx << 32) | binding;
            auto bufIt = m_buffers.find(key);
            if (bufIt != m_buffers.end()) {
                std::memcpy(bufIt->second.mappedData, block.data(), block.size());
                block.clearDirty();
                LOG_INFO("Uploaded block '{}' ({} bytes)", name, block.size());
            }
            else {
                // ✅ binding 在当前 shader 中不存在，保留 dirty 等待下次热重载
                LOG_INFO("Binding not present for block '{}', deferring upload (data preserved)", name);
            }
        }

        // 持久化备份所有块数据
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
        // 备份 uniform 块数据（以便后续恢复）
        backupBlockValues();

        // ─── 销毁旧描述符池（连同它分配的所有描述符集）───
        // 注意：外部已经通过 waitIdle 确保 GPU 空闲，此时销毁池安全
        m_resMgr->destroy(m_pool);

        // 创建新池（配置与构造函数相同）
        RHI::DescriptorPoolDesc poolDesc;
        poolDesc.maxSets = 8;
        poolDesc.poolSizes = {
            { RHI::DescriptorType::UniformBuffer, 4 },
            { RHI::DescriptorType::CombinedImageSampler, 8 },
            { RHI::DescriptorType::InputAttachment, 4 }
        };
        poolDesc.freeDescriptorSet = true;
        poolDesc.debugName = "MaterialPool";
        m_pool = m_resMgr->createDescriptorPool(poolDesc);
        // ────────────────────────────────────────────────

        // 清空描述符集映射（旧句柄已随池失效）
        RHI::DescriptorSetHandle globalSet;
        if (auto it = m_sets.find(0); it != m_sets.end())
            globalSet = it->second;
        m_sets.clear();
        if (globalSet.isValid()) m_sets[0] = globalSet;

        // 更新布局引用（模板可能已变）
        m_layouts = m_template->getLayouts();

        // 重建反射缓存（会重新创建 UBO、纹理绑定等）
        invalidateReflectionCache();
        buildReflectionCache();

        // 恢复之前备份的 uniform 值
        restoreBlockValues();

        // 恢复之前缓存的纹理绑定（即使当前 Shader 中不存在，也会在后续检测到后自动绑定）
        restoreCachedBindings();

        // 上传所有脏块到新描述符集
        applyAllDirtyBlocks();
    }

    void MaterialInstance::invalidateReflectionCache() {
        // 延迟销毁所有旧的 UBO 缓冲
        for (auto& [key, bufRes] : m_buffers) {
            m_resMgr->scheduleDestroy([buf = bufRes.buffer, resMgr = m_resMgr]() {
                resMgr->destroy(buf);
                }, 2);
        }
        m_buffers.clear();
        m_blocks.clear();
        m_blockBindings.clear();
        m_samplerBindings.clear();
        m_inputAttachmentBindings.clear();
        m_reflectionCached = false;

        // ⚠️ 注意：以下两项是持久数据，绝对不能清理！
        //   - m_savedBlockData   → uniform 数据持久层
        //   - m_blockLayouts     → 手动注册的布局持久层
        // 它们在 recreateDescriptorSets() 的 backupBlockValues() / getBlock() 中维护
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
                // 完美匹配：直接拷贝
                std::memcpy(block.data(), savedIt->second.data(), blockSize);
                block.markDirty();
                LOG_INFO("Restored block '{}', size={} (exact match)", name, blockSize);
            }
            else {
                // ✅ 宽松匹配：取较小值，超出部分零填充
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
        size_t blockSize) {

        uint64_t key = ((uint64_t)setIdx << 32) | binding;

        // 已存在则不重复创建
        if (m_buffers.find(key) != m_buffers.end()) return;

        RHI::BufferDesc bufDesc;
        bufDesc.size = blockSize;
        bufDesc.type = RHI::BufferType::Uniform;
        bufDesc.memoryType = RHI::MemoryType::CPU_To_GPU;
        bufDesc.allowUpdate = true;
        bufDesc.persistentMapped = true;
        bufDesc.debugName = blockName;

        auto buffer = m_resMgr->createBuffer(bufDesc);
        if (!buffer.isValid()) {
            LOG_ERROR("Failed to create GPU buffer for block '{}'", blockName);
            return;
        }

        auto* bufferObj = m_resMgr->getBuffer(buffer);
        if (!bufferObj) return;

        void* mapped = bufferObj->map();
        m_buffers[key] = { buffer, mapped, blockSize };

        // 绑定到描述符集
        auto set = getOrCreateSet(setIdx);
        auto* setObj = m_resMgr->getDescriptorSet(set);
        if (setObj) {
            setObj->writeBuffer(binding, 0, bufferObj, 0, blockSize);
            setObj->update();
            LOG_INFO("Created GPU buffer for '{}', size={}, set={}, binding={}",
                blockName, blockSize, setIdx, binding);
        }
        else {
            LOG_ERROR("Failed to get descriptor set {} for block '{}'", setIdx, blockName);
        }
    }
}