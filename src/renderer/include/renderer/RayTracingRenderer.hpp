#pragma once
#include <renderer/BaseRenderer.hpp>

namespace StarryEngine {
    // ── 路径追踪渲染器：软件光线追踪技术 ──
    // 无光栅场景分析（RT 场景在 shader 常量 / 未来的 RTPathSceneBuilder 场景缓冲内），
    // 仅托管 RayTracingRenderPath 并驱动帧循环；全局描述符/并行录制等共享逻辑在 BaseRenderer。
    class RayTracingRenderer : public BaseRenderer {
    public:
        using BaseRenderer::BaseRenderer;

    protected:
        // 无场景分析：仅按需重建渲染图（首次/尺寸变化）
        void buildSceneResources() override {
            if (m_needRebuildGraph) {
                doRebuildRenderGraph();
                m_needRebuildGraph = false;
            }
        }
    };
}
