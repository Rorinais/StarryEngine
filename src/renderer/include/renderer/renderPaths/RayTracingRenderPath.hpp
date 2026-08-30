#pragma once
#include <renderer/renderPaths/BaseRenderPath.hpp>

namespace StarryEngine {
    // ── 软件路径追踪渲染路径（GAMES202：RT → 时域累积 → 自适应 a-trous 降噪）──
    // 场景内置于 raytrace.comp（康奈尔盒常量）；相机经 push constants 逐帧传入。
    class RayTracingRenderPath : public BaseRenderPath {
    public:
        struct Config {
            uint32_t spp = 16;
            bool denoise = true;    // 交互/离线降噪：RT→累积→降噪→呈现（读 SceneColorFiltered）
            bool offline = false;   // 离线原始：跳过累积/降噪，呈现直接读 SceneColor
        };

        RayTracingRenderPath(std::shared_ptr<RHI::IRHI> rhi, uint32_t width, uint32_t height, const Config& cfg);

    protected:
        void buildConfigPasses(std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap) override;
        void doRebuildResources(const AnalysisSceneResult& sceneData) override {}
        void setConfig(const RenderPathConfig&) override {}
        void setDrawItems(const AnalysisSceneResult&) override {}
        void onAfterCompile() override;

    private:
        Config m_cfg;
    };
}
