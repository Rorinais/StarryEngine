#include "ImGuiExecutor.hpp"
#include "ImGuiManager.hpp"

namespace StarryEngine {

    ImGuiExecutor::ImGuiExecutor(ImGuiManager* manager)
        : m_manager(manager) {
    }

    void ImGuiExecutor::execute(
        RHI::RHICommandEncoder* encoder,
        const RenderContext& rctx,
        const PassContext& pctx,
        uint32_t subpassIndex)
    {
        if (m_manager) {
            m_manager->render(encoder, pctx.getFrameIndex());
        }
    }

    void ImGuiExecutor::clearDrawItems() {
        m_emptyItems.clear();
    }

    void ImGuiExecutor::addDrawItem(std::shared_ptr<DrawItem>) {}

    std::vector<std::shared_ptr<DrawItem>>& ImGuiExecutor::getDrawItems() {
        return m_emptyItems;
    }

} // namespace StarryEngine