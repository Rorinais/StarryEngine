#include <app/AppConfig.hpp>
#include <cstdlib>
#include <cstring>
#include <string>

namespace StarryEngine::App {

AppConfig AppConfig::load(int argc, char** argv) {
    AppConfig cfg;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        auto next = [&](const char*& out) {
            if (i + 1 < argc) { out = argv[++i]; return true; }
            return false;
        };
        const char* v = nullptr;
        if (std::strcmp(a, "--width") == 0 && next(v))         cfg.window.width  = static_cast<uint32_t>(std::strtoul(v, nullptr, 10));
        else if (std::strcmp(a, "--height") == 0 && next(v))   cfg.window.height = static_cast<uint32_t>(std::strtoul(v, nullptr, 10));
        else if (std::strcmp(a, "--title") == 0 && next(v))    cfg.window.title  = v;
        else if (std::strcmp(a, "--scene") == 0 && next(v))    cfg.sceneId       = v;
        else if (std::strcmp(a, "--transparent") == 0)         cfg.window.transparent = true;
        else if (std::strcmp(a, "--borderless") == 0)          cfg.window.borderless  = true;
        else if (std::strcmp(a, "--dump") == 0 && next(v)) {
            cfg.dumpFrames = true; cfg.dumpDir = v;
        }
        else if (std::strcmp(a, "--dump-count") == 0 && next(v))  cfg.dumpCount = static_cast<uint32_t>(std::strtoul(v, nullptr, 10));
        else if (std::strcmp(a, "--dump-index") == 0 && next(v))  cfg.dumpIndex = std::atoi(v);
        else if (std::strcmp(a, "--dump-exit") == 0)              cfg.exitAfterDump = true;
        else if (std::strcmp(a, "--help") == 0 || std::strcmp(a, "-h") == 0) {
            cfg.valid = false;   // 由调用方打印用法
        }
    }
    if (cfg.window.width < 2 || cfg.window.height < 2) cfg.valid = false;
    return cfg;
}

} // namespace StarryEngine::App
