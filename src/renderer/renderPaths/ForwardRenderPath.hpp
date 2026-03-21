#pragma once
#include "IRenderPath.hpp"
#include "../graph/RenderGraph.hpp"
#include "../../scene/Scene.hpp"
#include "RenderPathConfig.hpp"
#include <vector>
#include <memory>
#include <unordered_map>

namespace StarryEngine {

    class ForwardRenderPath : public IRenderPath {
    public:
        ForwardRenderPath(std::shared_ptr<RHI::IRHI> rhi,
            uint32_t width, uint32_t height);
        ~ForwardRenderPath() = default;

        void setConfig(const RenderPathConfig& config) override;
        bool initialize() override;
        void setDrawItems(const Scene::AnalysisSceneResult& sceneData) override;
        void update(const glm::mat4& view, const glm::mat4& proj, float deltaTime) override;
        void render(RHI::RHICommandEncoder* encoder, uint32_t frameIndex) override;
        void onResize(uint32_t width, uint32_t height) override;
        std::shared_ptr<RenderGraph::RenderGraph> getRenderGraph() { return m_renderGraph; }
        void setTextureDescs(const std::unordered_map<std::string, RHI::TextureDesc>& descs);
    private:
        bool buildGraph();
        void distributeDrawItems(const Scene::AnalysisSceneResult& sceneData);

        std::shared_ptr<RHI::IRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;
        uint32_t m_width, m_height;

        RenderPathConfig m_config;
        std::shared_ptr<RenderGraph::RenderGraph> m_renderGraph;

        // 存储每个 stage 对应的 PassNode 指针（用于编译后获取 RenderPass）
        std::unordered_map<Scene::RenderStage, RenderGraph::PassNode*> m_stagePassNode;

        // 存储每个 stage 的 RenderPass 句柄和 queue -> subpass 映射
        struct StagePassInfo {
            RHI::RenderPassHandle renderPassHandle;
            std::unordered_map<Scene::RenderQueue, uint32_t> queueToSubpass;
        };
        std::unordered_map<Scene::RenderStage, StagePassInfo> m_stagePassInfo;

        // 存储每个子通道的录制器，键为 (stage, subpass) 的组合（64位）
        std::unordered_map<uint64_t, std::shared_ptr<RenderGraph::ISubpassRecorder>> m_subpassRecorders;

        // 缓存场景分析结果（用于 update 中获取 PSO）
        std::shared_ptr<Scene::AnalysisSceneResult> m_cachedSceneData;

        // 纹理名称 -> 纹理描述（用于创建）
        std::unordered_map<std::string, RHI::TextureDesc> m_textureDescs;

        // 纹理名称 -> 当前虚拟纹理 ID（每次构建后填充）
        std::unordered_map<std::string, RenderGraph::TextureId> m_textureIdMap;

        // 交换链纹理特殊处理
        std::string m_swapchainTextureName = "Swapchain";
    };

} // namespace StarryEngine