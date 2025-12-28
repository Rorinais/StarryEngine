#include "RHI_INTERFACE.hpp"
#include <cmath>
#include <algorithm>
#include <random>
#include <cstring>
#include <array>
#include <fstream>
#include <sstream>

namespace StarryEngine::RHI {

    // ==================== 回调管理器 ====================
    class RHICallbackManager {
    public:
        void registerFrameCallback(FrameCallback callback);
        void unregisterFrameCallback(FrameCallback callback);

        void registerResizeCallback(ResizeCallback callback);
        void unregisterResizeCallback(ResizeCallback callback);

        void registerErrorCallback(ErrorCallback callback);
        void unregisterErrorCallback(ErrorCallback callback);

        void registerDebugCallback(DebugCallback callback);
        void unregisterDebugCallback(DebugCallback callback);

        void triggerFrameCallbacks(FrameData& frameData);
        void triggerResizeCallbacks(uint32_t width, uint32_t height);
        void triggerErrorCallbacks(const std::string& error, bool fatal);
        void triggerDebugCallbacks(MessageSeverity severity, MessageSource source, const std::string& message);

    private:
        std::vector<FrameCallback> m_frameCallbacks;
        std::vector<ResizeCallback> m_resizeCallbacks;
        std::vector<ErrorCallback> m_errorCallbacks;
        std::vector<DebugCallback> m_debugCallbacks;
    };
}