#pragma once
#include <core/Window.hpp>
#include <renderer/backend/vulkan/FrameContext.hpp>

namespace StarryEngine {
    class FrameMonitor {
    public:
        FrameMonitor(Window::Ptr window, std::shared_ptr<FrameContext> frameContext, uint32_t flightFrame);
        void tick();
        void updateTitle();
        float getTime() const;
        float getDeltaTime() const { return m_deltaTime; }
        float getFPS() const { return m_fps; }
    private:
        Window::Ptr m_window;
        std::shared_ptr<FrameContext> m_frameContext;
        uint32_t m_flightFrame;
        std::chrono::high_resolution_clock::time_point m_startTime;
        std::chrono::high_resolution_clock::time_point m_lastFrameTime;
        float m_deltaTime;
        float m_fps;
        uint64_t m_frameCount;
        float m_fpsUpdateInterval;
        float m_lastFPSUpdate;
        double m_lastTitleUpdate;
    };
}