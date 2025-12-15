#pragma once
#include <memory>

namespace StarryEngine {
    class Window;
    class IRenderer {
    public:
        virtual ~IRenderer() = default;  // 使用默认实现

        virtual void init(std::shared_ptr<Window> window) = 0;
        virtual void shutdown() = 0;
        virtual void beginFrame() = 0;
        virtual void endFrame() = 0;
        virtual bool shouldClose() const = 0;
        virtual void pollEvents() = 0;
        virtual void render() = 0;
        virtual void onSwapchainRecreated() = 0;
    };

} // namespace StarryEngine