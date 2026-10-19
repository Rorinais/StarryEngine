#pragma once
#include <renderer/renderPaths/BaseRenderPath.hpp>
#include <renderer/passes/IPass.hpp>
#include <renderer/graph/RenderGraph.hpp>
#include <scene/Scene.hpp>
#include <renderer/RenderTypes.hpp>
#include <vector>
#include <memory>
#include <unordered_map>

namespace StarryEngine {
    class ImGuiManager;

    // ── 光栅渲染路径（延迟/前向）──
    // 路径自包含：内建标准 PBR 管线（阴影→不透明→后处理→模板→粒子）作为默认槽位，
    // demo 通过 insertPass/overrideSlot/setSlotEnabled 注入或调整；构造只传 Config。
    class DeferredRenderPath : public BaseRenderPath {
    public:
        /// 渲染层 = pass 注册表（见 RenderLayers）。这里只给"用哪些层"，
        /// 空 = 全部有 pass 的层（等价旧版全开）。
        /// 例：{ RenderLayers::Background, RenderLayers::Opaque, RenderLayers::Surface }
        struct Config {
            std::vector<int> layers;
        };

        DeferredRenderPath(std::shared_ptr<RHI::IRHI> rhi, uint32_t width, uint32_t height);
        DeferredRenderPath(std::shared_ptr<RHI::IRHI> rhi, uint32_t width, uint32_t height,const Config& cfg);
        ~DeferredRenderPath() override = default;

        void setConfig(const RenderPathConfig&) override;
        void setDrawItems(const AnalysisSceneResult& sceneData) override;

        void setImGuiManager(ImGuiManager* mgr, uint32_t imageCount);

    protected:
        void buildConfigPasses(std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap) override;

        void doRebuildResources(const AnalysisSceneResult& sceneData) override;
        void onAfterCompile() override;
        void onAfterCompileImGui() override;
        void onBeforeGraphDestroy() override;

    private:
        void updateMaterialTextures(const AnalysisSceneResult& sceneData);
        void buildDefaultPipeline();

        Config m_cfg;
        uint32_t m_imguiImageCount = 2;
        ImGuiManager* m_imguiManager = nullptr;
    };

} // namespace StarryEngine
