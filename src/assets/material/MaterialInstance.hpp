#pragma once
#include <memory>
#include <vector>
#include <unordered_map>
#include "../../renderer/interface/RHI_RESOURCE_MANAGER.hpp"
#include "MaterialTemplate.hpp"

namespace StarryEngine::Assets {
    class MaterialInstance {
    public:
        MaterialInstance(std::shared_ptr<MaterialTemplate> tmpl,
            RHI::DescriptorPoolHandle pool,
            RHI::ResourceManager* resMgr,
            RHI::DescriptorSetHandle globalSet = RHI::DescriptorSetHandle::Null());
        ~MaterialInstance() = default;

        void setUniform(uint32_t setIndex, uint32_t binding, const void* data, size_t size);
        void setTexture(uint32_t setIndex, uint32_t binding, RHI::TextureHandle texture, RHI::SamplerHandle sampler);

        void setCullMode(RHI::CullMode mode) { m_template->setCullMode(mode); }
        void setDepthTest(bool enable) { m_template->setDepthTest(enable); }
        void setDepthWrite(bool enable) { m_template->setDepthWrite(enable); }
        void setDepthCompareOp(RHI::CompareOp op) { m_template->setDepthCompareOp(op); }
        void setAttachments(std::vector<RHI::BlendAttachmentState> attachments) { m_template->setAttachments(attachments); }
        void setRenderQueue(Scene::RenderQueue queue) { m_template->setRenderQueue(queue); }
        void setRenderStage(Scene::RenderStage stage) { m_template->setRenderStage(stage); }
        void setDeferred(bool deferred) { m_template->setDeferred(deferred); }
        void enableTransparent(bool enable = true) { m_template->enableTransparent(enable); }
        void enableDepthTest(bool enable = true) { m_template->enableDepthTest(enable); }
        void enableDepthWrite(bool enable = true) { m_template->enableDepthWrite(enable); }
        void setDebugName(std::string debugName) { m_template->setDebugName(debugName); }

        RHI::CullMode getCullMode() { return m_template->getCullMode(); }
        RHI::FrontFace getFrontFace() { return m_template->getFrontFace(); }
        RHI::CompareOp getDethCompareOp() { return m_template->getDethCompareOp(); }
        std::vector<RHI::BlendAttachmentState> getAttachments() { return m_template->getAttachments(); }
        Scene::RenderQueue getRenderQueue() const { return m_template->getRenderQueue(); }
        Scene::RenderStage getRenderStage() const { return m_template->getRenderStage(); }
        bool isTransparent() const { return m_template->isTransparent(); }
        bool isDepthTestEnable() const { return m_template->isDepthTestEnable(); }
        bool isDepthWriteEnable() const { return m_template->isDepthWriteEnable(); }
        bool isDeferred() const { return m_template->isDeferred(); }
        std::string getDebugName() { return m_template->getDebugName(); }

        RHI::DescriptorSetHandle getSet(uint32_t setIndex) const;
        const std::unordered_map<uint32_t, RHI::DescriptorSetHandle>& getAllSets() const { return m_sets; }
        std::shared_ptr<MaterialTemplate> getTemplate() const { return m_template; }

        void setInstancingLayout(const InstancingLayout& layout);
        const InstancingLayout* getInstancingLayout() const;

        void addTextureDependency(const std::string& textureName, uint32_t set, uint32_t binding) {
            m_textureDependencies[textureName] = { set, binding };
        }

        const std::unordered_map<std::string, std::pair<uint32_t, uint32_t>>& getTextureDependencies() const {
            return m_textureDependencies;
        }
        
        RHI::DescriptorSetHandle getOrCreateSet(uint32_t setIndex);

    private:
        std::shared_ptr<MaterialTemplate> m_template;
        RHI::ResourceManager* m_resMgr;
        RHI::DescriptorPoolHandle m_pool;

        // 存储已分配的描述符集，键为 set 索引
        std::unordered_map<uint32_t, RHI::DescriptorSetHandle> m_sets;
        // 存储每个 set 索引对应的布局句柄
        std::unordered_map<uint32_t, RHI::DescriptorSetLayoutHandle> m_layouts;

        struct BufferResource {
            RHI::BufferHandle buffer;
            void* mappedData;
            size_t size;
        };
        std::unordered_map<uint64_t, BufferResource> m_buffers; // 键 = ((uint64_t)set << 32) | binding

        std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> m_textureDependencies;

        InstancingLayout m_instancingLayout;
        bool m_hasCustomInstancingLayout = false;
    };
}