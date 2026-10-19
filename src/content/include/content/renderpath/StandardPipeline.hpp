#pragma once
// L4 内容层 · 声明层：标准渲染管线声明入口（与 RenderPathRegistry 同目录）。
//
// 职责单一：把"标准延迟管线"声明到引擎的注册表里（幂等）：
//   · 层 → pass 工厂        → RenderLayers
//   · 名字 → 附件描述        → RenderTextures
// 真正的路径创建由 RenderPathRegistry 负责。
#include <renderer/renderPaths/LayerRegistry.hpp>
#include <renderer/renderPaths/TextureRegistry.hpp>

namespace StarryEngine::Content {

    class StandardPipeline {
    public:
        /// 声明标准延迟管线：系统层 pass（RenderLayers）+ 标准附件（RenderTextures）
        /// @return true = 本次确实声明了 pass（首次调用）；false = 之前已声明
        static bool declare(uint32_t width, uint32_t height);

    private:
        static void declareAttachments(uint32_t width, uint32_t height);
    };

} // namespace StarryEngine::Content
