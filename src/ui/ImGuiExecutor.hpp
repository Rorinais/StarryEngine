#pragma once

#include "../renderer/passes/Subpass.hpp"
#include <memory>
#include <vector>

namespace StarryEngine {

    class ImGuiManager;

    class ImGuiExecutor : public IPassExecutor {
    public:
        explicit ImGuiExecutor(ImGuiManager* manager);

        void execute(
            RHI::RHICommandEncoder* encoder,
            const RenderContext& rctx,
            const PassContext& pctx,
            uint32_t subpassIndex) override;

        void clearDrawItems() override;

        void addDrawItem(std::shared_ptr<Scene::DrawItem> item) override;

        std::vector<std::shared_ptr<Scene::DrawItem>>& getDrawItems() override;

        void setDrawItems(const std::vector<std::shared_ptr<Scene::DrawItem>>& items) override {}

        void setPipelineMapping(const std::unordered_map<uint32_t, RHI::PipelineHandle>& mapping) override {}

    private:
        ImGuiManager* m_manager;
        std::vector<std::shared_ptr<Scene::DrawItem>> m_emptyItems;
    };

} // namespace StarryEngine