#include "MaterialInstance.hpp"
#include <cstring>

namespace StarryEngine::Assets {

    MaterialInstance::MaterialInstance(std::shared_ptr<MaterialTemplate> tmpl,
        RHI::DescriptorPoolHandle pool,
        RHI::ResourceManager* resMgr,
        RHI::DescriptorSetHandle globalSet)
        : m_template(tmpl), m_resMgr(resMgr), m_pool(pool) {
        // 从模板获取布局映射
        m_layouts = tmpl->getLayouts();

        // 如果传入了全局 set，则存储它
        if (globalSet.isValid()) {
            m_sets[0] = globalSet;
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
        if (!set.isValid()) return;

        auto* setObj = m_resMgr->getDescriptorSet(set);
        if (!setObj) return;
        auto* textureObj = m_resMgr->getTexture(texture);
        auto* samplerObj = m_resMgr->getSampler(sampler);
        if (!textureObj || !samplerObj) return;

        setObj->writeTexture(binding, 0, textureObj, samplerObj, RHI::ImageLayout::ShaderReadOnly);
        setObj->update();
    }


    void MaterialInstance::setInstancingLayout(const InstancingLayout& layout) {
        m_instancingLayout = layout;
        m_hasCustomInstancingLayout = true;
    }

    // 获取有效的实例化布局（优先使用自定义，否则使用模板的）
    const InstancingLayout* MaterialInstance::getInstancingLayout() const {
        if (m_hasCustomInstancingLayout) return &m_instancingLayout;
        return m_template ? m_template->getInstancingLayout() : nullptr;
    }
}