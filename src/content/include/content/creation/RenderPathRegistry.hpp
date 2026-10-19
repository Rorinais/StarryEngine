#pragma once
// L4 内容层 · 创建层：渲染路径映射表（默认登记，不默认创建）
//
// 与 VertexLayout 的 DefaultSemanticLocation 同风格：
//   名字 → 工厂函数 的映射表；默认表只"登记"deferred / raytrace，
//   实际创建由外部显式调用 create()，并可用 registerPath(overwrite=true) 替换。
#include <renderer/renderPaths/BaseRenderPath.hpp>
#include <renderer/renderPaths/DeferredRenderPath.hpp>
#include <renderer/interface/IRHI.hpp>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace StarryEngine::Content {

    struct PathParams {
        uint32_t spp = 16;                 ///< 光追采样数（raytrace 用）
        bool     denoise = true;           ///< 降噪开关（raytrace 用）
        bool     offline = false;          ///< 离线模式（raytrace 用）
        DeferredRenderPath::Config deferred{}; ///< 延迟路径可调项(shadows/skybox/grid/...)
    };

    /// 注册标准延迟管线的系统层 pass（幂等）：shadow/forward/skybox+grid/stencil/particles
    /// —— 默认 pass 创建放这里（内容层），引擎的 RenderLayers 只做层表。
    void registerStandardPasses();

    using PathFactory = std::function<std::shared_ptr<BaseRenderPath>(
        std::shared_ptr<RHI::IRHI> rhi, uint32_t width, uint32_t height, const PathParams& params)>;

    class RenderPathRegistry {
    public:
        static RenderPathRegistry& instance();

        /// 注册/替换一条路径。“deferred”/“raytrace” 为内置名。
        void registerPath(const std::string& name, PathFactory factory, bool overwrite = false);

        /// 按名字创建路径；名字不存在返回 nullptr（不静默回退）
        std::shared_ptr<BaseRenderPath> create(const std::string& name,
            std::shared_ptr<RHI::IRHI> rhi, uint32_t width, uint32_t height,
            const PathParams& params = {});

        /// 当前可用的路径名（含用户自定义）
        std::vector<std::string> names() const;

    private:
        RenderPathRegistry();
        std::unordered_map<std::string, PathFactory> m_factories;
    };

} // namespace StarryEngine::Content
