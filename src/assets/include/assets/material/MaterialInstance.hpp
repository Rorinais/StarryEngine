#pragma once
#include <memory>
#include <vector>
#include <array>
#include <unordered_map>
#include <renderer/interface/IRHI.hpp>
#include <renderer/interface/RHIManager.hpp>
#include <assets/material/MaterialTemplate.hpp>
#include <assets/material/DefaultMaterialTemplate.hpp>
#include <assets/material/MaterialParameterBlock.hpp>

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

        MaterialParameterBlock* getBlock(const std::string& blockName);
        void registerBlockLayout(const std::string& blockName, const RHI::ResourceBinding& binding);
        void setTexture(const std::string& name, RHI::TextureHandle texture, RHI::SamplerHandle sampler);
        void addTextureDependency(const std::string& shaderVarName,const std::string& rgTextureName,ResourceDependencyType type);
        void applyAllDirtyBlocks(uint32_t slot = UINT32_MAX);

        // slot 参数：帧槽位（0/1，ADR-6 per-slot）。UINT32_MAX = 用 m_currentSlot（渲染器每帧设）。
        // 首调会建两槽 buffer+描述符并灌满初始数据（writtenOnce），之后的逐帧写只落指定槽。
        void setUniform(uint32_t setIndex, uint32_t binding, const void* data, size_t size, uint32_t slot = UINT32_MAX);
        void setStorageBuffer(uint32_t setIndex, uint32_t binding, const void* data, size_t size, uint32_t slot = UINT32_MAX);
        void setTexture(uint32_t setIndex, uint32_t binding, RHI::TextureHandle texture, RHI::SamplerHandle sampler);
        void setInputAttachment(uint32_t setIndex, uint32_t binding, RHI::TextureHandle texture, RHI::ImageLayout layout = RHI::ImageLayout::ShaderReadOnly);
        void addTextureDependency(const std::string& textureName,uint32_t set,uint32_t binding,ResourceDependencyType type = ResourceDependencyType::Sampler) {
            m_textureDependencies[textureName] = { set, binding, type };
        }

        void setCullMode(RHI::CullMode mode) { m_cullMode = mode; }
        void setDepthTest(bool enable) { m_depthTestEnable = enable; }
        void setDepthWrite(bool enable) { m_depthWriteEnable = enable; }
        void setDepthCompareOp(RHI::CompareOp op) { m_depthCompareOp = op; }
        // 模板测试（front/back 两个 StencilOpState；setStencilOps 里没显式设置的字段用 RHI 默认值）
        void setStencilTest(bool enable) { m_stencilTestEnable = enable; }
        void setStencilOps(RHI::StencilOpState front, RHI::StencilOpState back) {
            m_stencilFront = front; m_stencilBack = back;
        }
        void setAttachments(std::vector<RHI::BlendAttachmentState> attachments) { m_attachments = std::move(attachments); }
        void setDeferred(bool deferred) { m_isDeferred = deferred; }
        void enableTransparent(bool enable = true) { m_alphaBlend = enable; }
        void enableDepthTest(bool enable = true) { m_depthTestEnable = enable; }
        void enableDepthWrite(bool enable = true) { m_depthWriteEnable = enable; }
        void setDebugName(std::string debugName) { m_debugName = std::move(debugName); }
        const InstancingLayout* getInstancingLayout() const;
        void setInstancingLayout(const InstancingLayout& layout);

        RHI::CullMode getCullMode() const { return m_cullMode; }
        RHI::FrontFace getFrontFace() const { return m_frontFace; }
        RHI::CompareOp getDethCompareOp() const { return m_depthCompareOp; }
        const std::vector<RHI::BlendAttachmentState>& getAttachments() const { return m_attachments; }
        bool isTransparent() const { return m_alphaBlend; }
        bool isDepthTestEnable() const { return m_depthTestEnable; }
        bool isDepthWriteEnable() const { return m_depthWriteEnable; }
        bool isStencilTestEnable() const { return m_stencilTestEnable; }
        const RHI::StencilOpState& getStencilFront() const { return m_stencilFront; }
        const RHI::StencilOpState& getStencilBack() const { return m_stencilBack; }
        bool isDeferred() const { return m_isDeferred; }
        std::string getDebugName() const { return m_debugName; }

        // per-slot 描述符集：slot 版本返回该槽位的集；无 slot 版本建满两槽并返回槽 0 的
        //（兼容"建好后批量绑定"的调用方，见 SceneAnalyzer::buildDescriptorSetMap）。
        RHI::DescriptorSetHandle getOrCreateSet(uint32_t setIndex);
        RHI::DescriptorSetHandle getOrCreateSet(uint32_t setIndex, uint32_t slot);
        RHI::DescriptorSetHandle getSet(uint32_t setIndex, uint32_t slot) const;

        // 当前帧槽位（renderer 每帧开始 setCurrentSlot；UINT32_MAX 参数的兜底）
        void setCurrentSlot(uint32_t slot) { m_currentSlot = slot; }
        uint32_t getCurrentSlot() const { return m_currentSlot; }

        std::shared_ptr<MaterialTemplate> getTemplate() const { return m_template; }
        const std::unordered_map<std::string, DependencyInfo>& getTextureDependencies() const {return m_textureDependencies;}

        bool hasSubpassTag() const { return m_hasTag; }
        const std::string& getSubpassTag() const { return m_subpassTag; }
        void setSubpassTag(const std::string& tag) {
            m_subpassTag = tag;
            m_hasTag = true;
        }

        static std::shared_ptr<DefaultMaterialTemplate> createDefaultTemplate(
            std::shared_ptr<RHI::ResourceManager> resMgr, 
            RHI::DescriptorSetLayoutHandle globalSetLayout);

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

        // 描述符集管理。m_sets[setIdx] 是按帧槽位的 vector（ADR-6 per-slot）：
        // 集 0 是被渲染器 global 覆盖的废码（构造注入 globalSet，两槽同一句柄）；
        // 集 >=1 每槽一个，被绑定到 slot 对应的描述符。
        std::unordered_map<uint32_t, std::vector<RHI::DescriptorSetHandle>> m_sets;
        std::unordered_map<uint32_t, RHI::DescriptorSetLayoutHandle> m_layouts;

        struct BufferResource {
            RHI::BufferHandle buffer;
            void* mappedData = nullptr;
            size_t size = 0;
            RHI::BufferType type = RHI::BufferType::Uniform;
        };
        // 每帧写的缓冲 → 按帧槽位双份。writtenOnce 记录是否已灌满两槽首帧数据
        // （首调/首写须填两槽，否则另一槽第一次上屏无光照/无数据）。
        struct SlotBuffers {
            std::array<BufferResource, RHI::kMaxFramesInFlight> perSlot;
            bool writtenOnce = false;
        };
        std::unordered_map<uint64_t, SlotBuffers> m_buffers;
        std::unordered_map<std::string, DependencyInfo> m_textureDependencies;
        uint32_t m_currentSlot = 0;

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
            size_t blockSize, RHI::DescriptorType descriptorType);

        // 统一的 buffer 写入：按 binding 的 descriptor 类型建 UBO/SSBO
        void setBufferData(uint32_t setIndex, uint32_t binding,
            const void* data, size_t size, RHI::DescriptorType descriptorType,
            uint32_t slot = UINT32_MAX);
        uint32_t resolveSlot(uint32_t slot) const {
            return (slot == UINT32_MAX) ? m_currentSlot : slot;
        }
        RHI::DescriptorType resolveBindingDescriptorType(uint32_t setIndex, uint32_t binding) const;

        // ---------- 实例独立的渲染状态 ----------
        RHI::CullMode m_cullMode = RHI::CullMode::None;
        RHI::FrontFace m_frontFace = RHI::FrontFace::CounterClockwise;
        RHI::CompareOp m_depthCompareOp = RHI::CompareOp::Less;
        std::vector<RHI::BlendAttachmentState> m_attachments = { RHI::BlendAttachmentState{} };

        bool m_alphaBlend = false;
        bool m_depthTestEnable = true;
        bool m_depthWriteEnable = true;
        bool m_stencilTestEnable = false;
        RHI::StencilOpState m_stencilFront;
        RHI::StencilOpState m_stencilBack;
        bool m_isDeferred = false;

        std::string m_subpassTag;
        bool m_hasTag = false;

        std::string m_debugName;
    };
}