#pragma once
#include <cstdint>

namespace StarryEngine {

    enum class EventType : uint32_t {
        // 窗口/应用
        WindowClose,
        WindowResize,
        WindowFocus,
        WindowLostFocus,
        WindowMoved,

        // 输入
        KeyPressed,
        KeyReleased,
        KeyTyped,
        MouseMoved,
        MouseButtonPressed,
        MouseButtonReleased,
        MouseScrolled,
        CameraSwitch,

        // 渲染
        FrameStart,
        FrameEnd,
        SwapchainRecreated,
        ResolutionChanged,

        // 资源
        ResourceCreated,
        ResourceDestroyed,
        ResourceUpdated,

        // 调试
        DebugMessage,

        // 用户自定义起始
        UserEvent = 1000
    };

} // namespace StarryEngine