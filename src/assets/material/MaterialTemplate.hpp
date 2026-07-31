#pragma once
#include <stb_image.h>
#include <variant>
#include <map>
#include "../AssetType.hpp"
#include "../../scene/SceneType.hpp"
#include "../../assets/loader/ShaderLoader.hpp"
#include "../../assets/geometry/VertexLayout.hpp"

namespace StarryEngine::Assets {
    struct PipelineCacheKey {
        size_t stateHash;
        RHI::RenderPassHandle renderPass;
        uint32_t subpassIndex;

        bool operator==(const PipelineCacheKey& other) const {
            return stateHash == other.stateHash &&
                renderPass == other.renderPass &&
                subpassIndex == other.subpassIndex;
        }
    };
}

namespace std {
    template<> struct hash<StarryEngine::Assets::PipelineCacheKey> {
        size_t operator()(const StarryEngine::Assets::PipelineCacheKey& key) const noexcept {
            size_t seed = 0;
            StarryEngine::Utils::hash_combine(seed, key.stateHash);
            StarryEngine::Utils::hash_combine(seed, key.renderPass);
            StarryEngine::Utils::hash_combine(seed, key.subpassIndex);
            return seed;
        }
    };
}

namespace StarryEngine::Assets {
    class PipelineCache {
    public:
        using Key = PipelineCacheKey;

        static RHI::PipelineHandle getOrCreateGraphicsPipeline(
            RHI::ResourceManager* resMgr,
            const Scene::GraphicsPipelineState& state,
            RHI::RenderPassHandle renderPass,
            uint32_t subpassIndex);

        static void clearCache();
        static void invalidateAll(RHI::ResourceManager* resMgr);

    private:
        static std::unordered_map<Key, RHI::PipelineHandle> s_cache;
        static std::mutex s_mutex;
    };

    class DescriptorSetLayoutCache {
    public:
        static RHI::DescriptorSetLayoutHandle getOrCreateLayout(
            RHI::ResourceManager* resMgr,
            const RHI::DescriptorSetLayoutDesc& desc);

        static void clearCache();

    private:
        static std::unordered_map<size_t, RHI::DescriptorSetLayoutHandle> s_layoutCache;
    };


    class MaterialTemplate {
    public:
        virtual ~MaterialTemplate() = default;
        static void clearCache();

        void setCullMode(RHI::CullMode mode) { m_cullMode = mode; }
        void setDepthTest(bool enable) { m_depthTestEnable = enable; }
        void setDepthWrite(bool enable) { m_depthWriteEnable = enable; }
        void setDepthCompareOp(RHI::CompareOp op) { m_depthCompareOp = op; }
        void setAttachments(std::vector<RHI::BlendAttachmentState> attachment) { m_attachments = attachment; }
        void setDeferred(bool deferred) { m_isDeferred = deferred; }
        void enableTransparent(bool enable = true) { m_alphaBlend = enable; }
        void enableDepthTest(bool enable = true) { m_depthTestEnable = enable; }
        void enableDepthWrite(bool enable = true) { m_depthWriteEnable = enable; }
        virtual void setDebugName(std::string debugName) { m_debugName = debugName; }

        RHI::CullMode getCullMode() { return m_cullMode; }
        RHI::FrontFace getFrontFace() { return m_frontFace; }
        RHI::CompareOp getDethCompareOp() { return m_depthCompareOp; }
        std::vector<RHI::BlendAttachmentState> getAttachments() { return m_attachments; }
        virtual std::string getDebugName() { return m_debugName; }
        bool isTransparent() const { return m_alphaBlend; }
        bool isDepthTestEnable() const { return m_depthTestEnable; }
        bool isDepthWriteEnable() const { return m_depthWriteEnable; }
        bool isDeferred() const { return m_isDeferred; }

        virtual RHI::ShaderHandle getVertexShader() const = 0;
        virtual RHI::ShaderHandle getFragmentShader() const = 0;
        virtual const RHI::ShaderReflectionInfo& getVSReflection() const = 0;
        virtual const RHI::ShaderReflectionInfo& getFSReflection() const = 0;

        virtual std::vector<RHI::PushConstantRange> getPushConstants() const = 0;
        RHI::PipelineLayoutHandle getPipelineLayout(RHI::ResourceManager* resMgr);
        virtual const InstancingLayout* getInstancingLayout() const { return nullptr; }
        virtual std::unordered_map<uint32_t, RHI::DescriptorSetLayoutHandle> getLayouts() const = 0;

    private:
        static std::unordered_map<size_t, RHI::PipelineLayoutHandle> s_layoutCache;
        bool m_hasCustomLayout = false;
        std::string m_debugName = " ";

        RHI::CullMode m_cullMode = RHI::CullMode::None;
        RHI::FrontFace m_frontFace = RHI::FrontFace::CounterClockwise;
        RHI::CompareOp m_depthCompareOp = RHI::CompareOp::Less;
        std::vector<RHI::BlendAttachmentState> m_attachments = { RHI::BlendAttachmentState{} };

        bool m_alphaBlend = false;
        bool m_depthTestEnable = true;
        bool m_depthWriteEnable = true;
        bool m_isDeferred = false;
    };
}