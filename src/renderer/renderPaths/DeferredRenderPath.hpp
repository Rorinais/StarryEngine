#pragma once
#include "BaseRenderPath.hpp"
#include "../passes/IPass.hpp"
#include "../graph/RenderGraph.hpp"
#include "../../scene/Scene.hpp"
#include "../RenderTypes.hpp"
#include <vector>
#include <memory>
#include <unordered_map>

namespace StarryEngine {
    class ImGuiManager;

    class DeferredRenderPath : public BaseRenderPath {
    public:
        DeferredRenderPath(std::shared_ptr<RHI::IRHI> rhi, uint32_t width, uint32_t height);
        ~DeferredRenderPath() = default;

        // IRenderPath — deferred-specific overrides
        void setConfig(const RenderPathConfig&) override;  
        void setDrawItems(const AnalysisSceneResult& sceneData) override;

        void setImGuiManager(ImGuiManager* mgr, uint32_t imageCount);

    protected:
        void buildConfigPasses(std::unordered_map<std::string, RenderGraph::TextureId>& texIdMap) override;

        void doRebuildResources(const AnalysisSceneResult& sceneData) override;
        void onAfterCompile() override;
        void onAfterCompileImGui() override;

    private:
        void updateMaterialTextures(const AnalysisSceneResult& sceneData);

        uint32_t m_imguiImageCount = 2;
        ImGuiManager* m_imguiManager = nullptr;
    };

} // namespace StarryEngine
