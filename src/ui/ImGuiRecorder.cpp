#include "ImGuiRecorder.hpp"
#include "ImGuiManager.hpp"

namespace StarryEngine {

    ImGuiRecorder::ImGuiRecorder(ImGuiManager* manager)
        : m_manager(manager) {
    }

    void ImGuiRecorder::recordCommands(
        RHI::RHICommandEncoder* encoder,
        const RenderContext& rctx,
        const PassContext& pctx,
        uint32_t subpassIndex)
    {
        if (m_manager) {
            m_manager->render(encoder, pctx.getFrameIndex());
        }
    }

    void ImGuiRecorder::clearDrawItems() {
        m_emptyItems.clear();
    }

    void ImGuiRecorder::addDrawItem(std::shared_ptr<Scene::DrawItem>) {}

    std::vector<std::shared_ptr<Scene::DrawItem>>& ImGuiRecorder::getDrawItems() {
        return m_emptyItems;
    }

} // namespace StarryEngine