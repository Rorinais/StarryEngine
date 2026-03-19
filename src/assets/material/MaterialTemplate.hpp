#pragma once
#include <stb_image.h>
#include <variant>
#include "../AssetType.hpp"
#include "../../scene/SceneType.hpp"
#include "../../assets/loader/ShaderLoader.hpp"

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
        virtual std::vector<RHI::DescriptorSetLayoutHandle> getLayouts() const = 0;

        virtual std::vector<RHI::PushConstantRange> getPushConstants() const = 0;

        RHI::PipelineLayoutHandle getPipelineLayout(RHI::ResourceManager* resMgr);

        static void clearCache();

    private:
        //全局渲染管线布局缓存，将管线描述hash，作为键，因为管线描述之和描述符布局与常量推送布局有关系
        //如果以创建相同的管线布局，则使用缓存中的布局，否则通过描述符布局生成创建新的管线布局
        static std::unordered_map<size_t, RHI::PipelineLayoutHandle> s_layoutCache;
    };
}

