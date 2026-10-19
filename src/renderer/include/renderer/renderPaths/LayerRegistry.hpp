#pragma once
// 渲染层表（Unity Layers 风格）：索引固定、名字固定/用户自定义。
//
// 结构：
//   0 Background  — 阴影/背景前置（系统）
//   1 Opaque      — 不透明（系统）
//   2 Surface     — 天空盒 / 网格地面（系统）
//   3 Outline     — 描边（系统）
//   4 Particles   — 粒子/透明（系统）
//   5..7          — 系统保留
//   8..31         — 用户自定义层（setUserLayer 命名）
//
// pass 挂在层上，渲染路径按层索引升序执行。
#include <renderer/passes/GraphicsPass.hpp>
#include <functional>
#include <array>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace StarryEngine {

    /// pass 构建上下文（给层工厂用）
    struct PassBuildContext {
        uint32_t width = 0;
        uint32_t height = 0;
        std::function<void(int layer, std::shared_ptr<IPass>, const std::string&)> setSlot;
        std::function<std::shared_ptr<GraphicsPass>(const std::string&)> getGroupPass;
        std::unordered_set<std::string> slotTags;
    };

    using PassFactory = std::function<void(PassBuildContext&)>;

    class RenderLayers {
    public:
        enum : int {
            Background  = 0,   ///< 阴影/背景前置
            Opaque      = 1,   ///< 不透明
            Surface     = 2,   ///< 天空盒 / 网格地面
            Outline     = 3,   ///< 描边(stencil)
            Particles   = 4,   ///< 粒子/透明
            FirstUser   = 8,   ///< 用户自定义起点（Unity 同样从 8 开始）
            LayerCount  = 32,
        };

        static RenderLayers& instance();

        /// 系统层名（0..7；保留位为空字符串）
        static const char* builtinName(int layer);

        /// 用户自定义层：索引必须在 [FirstUser, LayerCount)
        bool setUserLayer(int index, const std::string& name);
        /// 用户自定义层：自动挑一个空位
        int  addUserLayer(const std::string& name);

        /// 名字 → 索引（系统名 + 用户层名；未知返回 -1）
        int find(const std::string& name) const;
        /// 索引 → 名字
        std::string nameOf(int index) const;
        /// 全部已命名层，按索引升序
        std::vector<std::pair<int, std::string>> all() const;

        // ── 层 = pass 注册表：索引 → 工厂 ──
        /// 把 pass 工厂绑到层索引（overwrite=false 时已存在则告警不覆盖）
        void setPass(int layer, PassFactory factory, bool overwrite = false);
        const PassFactory* passOf(int layer) const;
        std::vector<int> layersWithPass() const;
        /// 依次执行各层工厂构建 pass，then 清空（构建是收集，不应残留）
        void build(PassBuildContext& ctx);

    private:
        RenderLayers();
        std::array<std::string, LayerCount> m_names{};
        std::array<PassFactory, LayerCount> m_passes{};
    };

} // namespace StarryEngine
