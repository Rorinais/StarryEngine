#pragma once
// ────────────────────────────────────────────────────────────────────────
// L3 应用层：启动配置。
// 描述"这次运行要什么"（窗口参数、选哪个场景），不包含任何内容与渲染细节。
// 目前以代码默认值为准，预留命令行覆盖；后续可接 JSON。
// ────────────────────────────────────────────────────────────────────────
#include <cstdint>
#include <string>

namespace StarryEngine::App {

struct WindowConfig {
    uint32_t width       = 800;
    uint32_t height      = 600;
    std::string title    = "StarryEngine";
    bool     transparent = false;
    bool     resizable   = true;
    bool     borderless  = false;
    bool     alwaysOnTop = false;
    bool     highDPI     = false;
};

struct AppConfig {
    WindowConfig window;
    std::string  sceneId = "pbr";   ///< "pbr" | "raytrace"（由 SceneSwitcher 使用）
    bool         valid   = true;

    // 帧捕获（诊断用；替代原 STARRY_FRAME_DUMP 系列环境变量）
    bool         dumpFrames  = false;
    std::string  dumpDir;
    uint32_t     dumpCount   = 1;
    int          dumpIndex   = -1;     ///< <0 = 不指定
    bool         exitAfterDump = false;

    /// 读取配置：默认值 + 命令行覆盖（--width/--height/--scene）
    static AppConfig load(int argc, char** argv);
};

} // namespace StarryEngine::App
