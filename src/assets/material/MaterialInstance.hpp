#pragma once
#include <memory>
#include <vector>
#include <unordered_map>
#include "../../renderer/interface/RHI_RESOURCE_MANAGER.hpp"
#include "MaterialTemplate.hpp"
#include "DefaultMaterialTemplate.hpp"
#include "MaterialParameterBlock.hpp"

namespace StarryEngine::Assets {
    enum class ResourceDependencyType {
        Sampler,
        InputAttachment,
        StorageImage,
        UniformBuffer,
    };

    struct DependencyInfo {
        uint32_t set;
        uint32_t binding;
        ResourceDependencyType type;
    };

    class MaterialInstance {
    public:
        MaterialInstance(std::shared_ptr<MaterialTemplate> tmpl,
            RHI::DescriptorPoolHandle pool,
            RHI::ResourceManager* resMgr,
            RHI::DescriptorSetHandle globalSet = RHI::DescriptorSetHandle::Null());
        ~MaterialInstance();

        MaterialInstance(const MaterialInstance&) = delete;
        MaterialInstance& operator=(const MaterialInstance&) = delete;
        MaterialInstance(MaterialInstance&&) = delete;
        MaterialInstance& operator=(MaterialInstance&&) = delete;

        void recreateDescriptorSets();
        void invalidateReflectionCache();

        void setUniform(uint32_t setIndex, uint32_t binding, const void* data, size_t size);
        void setTexture(uint32_t setIndex, uint32_t binding, RHI::TextureHandle texture, RHI::SamplerHandle sampler);
        void setInputAttachment(uint32_t setIndex, uint32_t binding, RHI::TextureHandle texture, RHI::ImageLayout layout = RHI::ImageLayout::ShaderReadOnly);

        MaterialParameterBlock* getBlock(const std::string& blockName);
        void registerBlockLayout(const std::string& blockName, const RHI::ResourceBinding& binding);
        void setTexture(const std::string& name, RHI::TextureHandle texture, RHI::SamplerHandle sampler);
        void addTextureDependency(const std::string& shaderVarName,const std::string& rgTextureName,ResourceDependencyType type);
        void applyAllDirtyBlocks();

        // 实例独立的渲染状态 setter / getter
        void setCullMode(RHI::CullMode mode) { m_cullMode = mode; }
        void setDepthTest(bool enable) { m_depthTestEnable = enable; }
        void setDepthWrite(bool enable) { m_depthWriteEnable = enable; }
        void setDepthCompareOp(RHI::CompareOp op) { m_depthCompareOp = op; }
        void setAttachments(std::vector<RHI::BlendAttachmentState> attachments) { m_attachments = std::move(attachments); }
        void setDeferred(bool deferred) { m_isDeferred = deferred; }
        void enableTransparent(bool enable = true) { m_alphaBlend = enable; }
        void enableDepthTest(bool enable = true) { m_depthTestEnable = enable; }
        void enableDepthWrite(bool enable = true) { m_depthWriteEnable = enable; }
        void setDebugName(std::string debugName) { m_debugName = std::move(debugName); }

        RHI::CullMode getCullMode() const { return m_cullMode; }
        RHI::FrontFace getFrontFace() const { return m_frontFace; }
        RHI::CompareOp getDethCompareOp() const { return m_depthCompareOp; }
        const std::vector<RHI::BlendAttachmentState>& getAttachments() const { return m_attachments; }
        bool isTransparent() const { return m_alphaBlend; }
        bool isDepthTestEnable() const { return m_depthTestEnable; }
        bool isDepthWriteEnable() const { return m_depthWriteEnable; }
        bool isDeferred() const { return m_isDeferred; }
        std::string getDebugName() const { return m_debugName; }

        RHI::DescriptorSetHandle getSet(uint32_t setIndex) const;
        const std::unordered_map<uint32_t, RHI::DescriptorSetHandle>& getAllSets() const { return m_sets; }
        std::shared_ptr<MaterialTemplate> getTemplate() const { return m_template; }

        void setInstancingLayout(const InstancingLayout& layout);
        const InstancingLayout* getInstancingLayout() const;

        void addTextureDependency(const std::string& textureName,
            uint32_t set,
            uint32_t binding,
            ResourceDependencyType type = ResourceDependencyType::Sampler) {
            m_textureDependencies[textureName] = { set, binding, type };
        }

        const std::unordered_map<std::string, DependencyInfo>& getTextureDependencies() const {
            return m_textureDependencies;
        }

        void setSubpassTag(const std::string& tag) {
            m_subpassTag = tag;
            m_hasTag = true;
        }
        const std::string& getSubpassTag() const { return m_subpassTag; }
        bool hasSubpassTag() const { return m_hasTag; }

        RHI::DescriptorSetHandle getOrCreateSet(uint32_t setIndex);

        static std::shared_ptr<DefaultMaterialTemplate> createDefaultTemplate(std::shared_ptr<RHI::ResourceManager> resMgr, RHI::DescriptorSetLayoutHandle globalSetLayout);

        static std::shared_ptr<MaterialInstance> createDefault(
            std::shared_ptr<RHI::ResourceManager> resMgr,
            RHI::DescriptorSetLayoutHandle globalSetLayout,
            RHI::DescriptorPoolHandle pool,
            RHI::DescriptorSetHandle globalSet);

        static std::shared_ptr<MaterialInstance> createError(
            std::shared_ptr<RHI::ResourceManager> resMgr,
            RHI::DescriptorSetLayoutHandle globalSetLayout,
            RHI::DescriptorPoolHandle pool,
            RHI::DescriptorSetHandle globalSet);

    private:
        std::shared_ptr<MaterialTemplate> m_template;
        RHI::ResourceManager* m_resMgr;
        RHI::DescriptorPoolHandle m_pool;

        // 描述符集管理
        std::unordered_map<uint32_t, RHI::DescriptorSetHandle> m_sets;
        std::unordered_map<uint32_t, RHI::DescriptorSetLayoutHandle> m_layouts;

        struct BufferResource {
            RHI::BufferHandle buffer;
            void* mappedData;
            size_t size;
        };
        std::unordered_map<uint64_t, BufferResource> m_buffers;
        std::unordered_map<std::string, DependencyInfo> m_textureDependencies;

        InstancingLayout m_instancingLayout;
        bool m_hasCustomInstancingLayout = false;

        // 反射缓存
        std::unordered_map<std::string, MaterialParameterBlock> m_blocks;
        std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> m_blockBindings;    // 块名 -> (set,binding)
        std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> m_samplerBindings;  // 纹理名 -> (set,binding)
        std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> m_inputAttachmentBindings;
        std::unordered_map<std::string, std::vector<uint8_t>> m_savedBlockData;
        bool m_reflectionCached = false;

        struct CachedTexture {
            uint32_t set = UINT32_MAX;
            uint32_t binding = UINT32_MAX;
            RHI::TextureHandle texture;
            RHI::SamplerHandle sampler;
            bool valid() const { return set != UINT32_MAX && binding != UINT32_MAX; }
        };
        std::unordered_map<std::string, CachedTexture> m_cachedTextures;
        std::unordered_map<std::string, std::vector<uint8_t>> m_blockBackups;
        std::unordered_map<std::string, RHI::ResourceBinding> m_blockLayouts;
        void backupBlockValues();
        void restoreBlockValues();
        void restoreCachedBindings();
        void buildReflectionCache();
        void ensureGPUBufferForBlock(const std::string& blockName,
            uint32_t setIdx, uint32_t binding,
            size_t blockSize);

        // ---------- 实例独立的渲染状态 ----------
        RHI::CullMode m_cullMode = RHI::CullMode::None;
        RHI::FrontFace m_frontFace = RHI::FrontFace::CounterClockwise;
        RHI::CompareOp m_depthCompareOp = RHI::CompareOp::Less;
        std::vector<RHI::BlendAttachmentState> m_attachments = { RHI::BlendAttachmentState{} };

        bool m_alphaBlend = false;
        bool m_depthTestEnable = true;
        bool m_depthWriteEnable = true;
        bool m_isDeferred = false;

        std::string m_subpassTag;
        bool m_hasTag = false;

        std::string m_debugName;
    };
}