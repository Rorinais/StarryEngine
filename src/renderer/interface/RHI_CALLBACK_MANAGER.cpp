#include "RHI_CALLBACK_MANAGER.hpp"

namespace StarryEngine::RHI {

    // ==================== RHICallbackManager 实现 ====================

    void RHICallbackManager::registerFrameCallback(FrameCallback callback) {
        if (callback) {
            m_frameCallbacks.push_back(callback);
        }
    }

    void RHICallbackManager::unregisterFrameCallback(FrameCallback callback) {
        auto it = std::remove(m_frameCallbacks.begin(), m_frameCallbacks.end(), callback);
        m_frameCallbacks.erase(it, m_frameCallbacks.end());
    }

    void RHICallbackManager::registerResizeCallback(ResizeCallback callback) {
        if (callback) {
            m_resizeCallbacks.push_back(callback);
        }
    }

    void RHICallbackManager::unregisterResizeCallback(ResizeCallback callback) {
        auto it = std::remove(m_resizeCallbacks.begin(), m_resizeCallbacks.end(), callback);
        m_resizeCallbacks.erase(it, m_resizeCallbacks.end());
    }

    void RHICallbackManager::registerErrorCallback(ErrorCallback callback) {
        if (callback) {
            m_errorCallbacks.push_back(callback);
        }
    }

    void RHICallbackManager::unregisterErrorCallback(ErrorCallback callback) {
        auto it = std::remove(m_errorCallbacks.begin(), m_errorCallbacks.end(), callback);
        m_errorCallbacks.erase(it, m_errorCallbacks.end());
    }

    void RHICallbackManager::registerDebugCallback(DebugCallback callback) {
        if (callback) {
            m_debugCallbacks.push_back(callback);
        }
    }

    void RHICallbackManager::unregisterDebugCallback(DebugCallback callback) {
        auto it = std::remove(m_debugCallbacks.begin(), m_debugCallbacks.end(), callback);
        m_debugCallbacks.erase(it, m_debugCallbacks.end());
    }

    void RHICallbackManager::triggerFrameCallbacks(FrameData& frameData) {
        for (auto& callback : m_frameCallbacks) {
            try {
                callback(frameData);
            }
            catch (...) {
                // 防止回调异常影响主流程
            }
        }
    }

    void RHICallbackManager::triggerResizeCallbacks(uint32_t width, uint32_t height) {
        for (auto& callback : m_resizeCallbacks) {
            try {
                callback(width, height);
            }
            catch (...) {
                // 防止回调异常影响主流程
            }
        }
    }

    void RHICallbackManager::triggerErrorCallbacks(const std::string& error, bool fatal) {
        for (auto& callback : m_errorCallbacks) {
            try {
                callback(error, fatal);
            }
            catch (...) {
                // 防止回调异常影响主流程
            }
        }
    }

    void RHICallbackManager::triggerDebugCallbacks(
        MessageSeverity severity,
        MessageSource source,
        const std::string& message) {

        for (auto& callback : m_debugCallbacks) {
            try {
                callback(severity, source, message);
            }
            catch (...) {
                // 防止回调异常影响主流程
            }
        }
    }

} // namespace StarryEngine::RHI