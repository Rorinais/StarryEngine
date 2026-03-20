#include"MaterialTemplate.hpp"

namespace StarryEngine::Assets {
    std::unordered_map<PipelineCache::Key, RHI::PipelineHandle> PipelineCache::s_cache;
    std::mutex PipelineCache::s_mutex;
    std::unordered_map<size_t, RHI::DescriptorSetLayoutHandle> DescriptorSetLayoutCache::s_layoutCache;
    std::unordered_map<size_t, RHI::PipelineLayoutHandle> MaterialTemplate::s_layoutCache;

    RHI::PipelineHandle PipelineCache::getOrCreateGraphicsPipeline(
        RHI::ResourceManager* resMgr,
        const Scene::GraphicsPipelineState& state,
        RHI::RenderPassHandle renderPass,
        uint32_t subpassIndex) {

        size_t stateHash = std::hash<Scene::GraphicsPipelineState>{}(state);
        Key key{ stateHash, renderPass, subpassIndex };

        // 线程安全加锁
        std::lock_guard<std::mutex> lock(s_mutex);

        auto it = s_cache.find(key);
        if (it != s_cache.end()) {
            return it->second;
        }

        RHI::GraphicsPipelineDesc desc;
        desc.vertexShader = state.vertexShader;
        desc.fragmentShader = state.fragmentShader;
        desc.vertexInput = state.vertexInput;
        desc.pipelineLayoutHandle = state.layout;
        desc.renderPass = renderPass;
        desc.subpass = subpassIndex;
        desc.rasterizer.cullMode = state.cullMode;
        desc.rasterizer.frontFace = state.frontFace;
        desc.rasterizer.lineWidth = state.lineWidth;
        desc.depthStencil.depthTestEnable = state.depthTestEnable;
        desc.depthStencil.depthWriteEnable = state.depthWriteEnable;
        desc.depthStencil.depthCompareOp = state.depthCompareOp;
        desc.topology = state.topology;
        desc.viewport.viewports = state.viewports;
        desc.viewport.scissors = state.scissors;
        desc.colorBlend.attachments = state.attachments;
        desc.dynamicStates = state.dynamicStates;

        auto pipeline = resMgr->createGraphicsPipeline(desc);
        s_cache[key] = pipeline;
        return pipeline;
    }

    void PipelineCache::clearCache() {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_cache.clear();
    }

    RHI::DescriptorSetLayoutHandle DescriptorSetLayoutCache::getOrCreateLayout(
        RHI::ResourceManager* resMgr,
        const RHI::DescriptorSetLayoutDesc& desc) {

        size_t hash = 0;
        Utils::hash_combine(hash, desc);

        auto it = s_layoutCache.find(hash);
        if (it != s_layoutCache.end()) {
            return it->second;
        }

        auto layout = resMgr->createDescriptorSetLayout(desc);
        s_layoutCache[hash] = layout;
        return layout;
    }

    void DescriptorSetLayoutCache::clearCache() {
        s_layoutCache.clear();
    }

    RHI::PipelineLayoutHandle MaterialTemplate::getPipelineLayout(RHI::ResourceManager* resMgr) {
        // 1. 获取布局映射
        auto layoutMap = getLayouts();
        // 2. 将 map 转换为按 set 索引升序的 vector
        std::vector<RHI::DescriptorSetLayoutHandle> orderedLayouts;
        orderedLayouts.reserve(layoutMap.size());
        // 按键排序
        std::map<uint32_t, RHI::DescriptorSetLayoutHandle> sortedMap(layoutMap.begin(), layoutMap.end());
        for (const auto& [setIdx, layout] : sortedMap) {
            orderedLayouts.push_back(layout);
        }

        // 3. 计算组合哈希：先以布局向量初始化种子，再混合推送常量向量
        size_t hash = 0;
        Utils::hash_combine(hash, orderedLayouts);
        Utils::hash_combine(hash, getPushConstants());

        // 4. 查找缓存
        auto it = s_layoutCache.find(hash);
        if (it != s_layoutCache.end()) {
            return it->second;
        }

        // 5. 创建新的 PipelineLayout
        RHI::PipelineLayoutDesc desc;
        desc.descriptorSetLayouts = orderedLayouts;
        desc.pushConstants = getPushConstants();
        auto layout = resMgr->createPipelineLayout(desc);
        s_layoutCache[hash] = layout;
        return layout;
    }

    void MaterialTemplate::clearCache() {
        s_layoutCache.clear();
    }
}