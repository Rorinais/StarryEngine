#pragma once
// ────────────────────────────────────────────────────────────────
// L4 内容层 · 创建层：场景/渲染器切换入口（原 SceneHost）。
//
// 本质是"切哪个渲染场景"：按配置选择 PBR 或光追渲染器 + 对应场景，
// 挂到 Application 并注册回调/诊断。设备丢失软重启时可重复调用重建。
// 只做装配与切换，不做任何渲染细节。
// ────────────────────────────────────────────────────────────────
#include <app/AppConfig.hpp>

namespace StarryEngine { class Application; }

namespace StarryEngine::Content {

class SceneSwitcher {
public:
    /// 按 cfg.sceneId 选择渲染场景并挂到 app（含 update/post 回调、退出回调、诊断）。
    /// 可在设备丢失恢复时重复调用（用新 RHI 重建内容）。
    static bool install(StarryEngine::Application& app, const StarryEngine::App::AppConfig& cfg);
};

} // namespace StarryEngine::Content
