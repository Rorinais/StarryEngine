#include <core/FrameMonitor.hpp>

namespace StarryEngine {
    FrameMonitor::FrameMonitor(Window::Ptr window, std::shared_ptr<VulkanFrameContext> frameContext, uint32_t flightFrame)
        : m_window(window)
        , m_frameContext(frameContext)
        , m_flightFrame(flightFrame)
        , m_startTime(std::chrono::high_resolution_clock::now())
        , m_lastFrameTime(m_startTime)
        , m_deltaTime(0.0f)
        , m_fps(0.0f)
        , m_frameCount(0)
        , m_lastFPSUpdate(0.0f)
        , m_fpsUpdateInterval(1.0f)
        , m_lastTitleUpdate(0.0) {
    }

    void FrameMonitor::tick() {
        auto currentTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> delta = currentTime - m_lastFrameTime;
        m_deltaTime = delta.count();
        m_lastFrameTime = currentTime;

        m_frameCount++;
        float now = getTime();
        if (now - m_lastFPSUpdate >= m_fpsUpdateInterval) {
            m_fps = static_cast<float>(m_frameCount) / (now - m_lastFPSUpdate);
            m_frameCount = 0;
            m_lastFPSUpdate = now;
        }
    }

    void FrameMonitor::updateTitle() {
        double now = getTime();
        if (now - m_lastTitleUpdate >= 1.0) {
            const auto& stats = m_frameContext->getStatistics();
            uint32_t lastFrameIdx = (m_frameContext->getCurrentFrameIndex() + m_flightFrame - 1) % m_flightFrame;
            float lastGpuTime = m_frameContext->getFrameGPUTime(lastFrameIdx);

            std::stringstream title;
            title << "StarryEngine"
                << " | FPS: " << std::fixed << std::setprecision(1) << m_fps
                << " | GPU Time: " << std::setprecision(3) << lastGpuTime << " ms"
                << " | CPU(avg): " << stats.averageCPUTime << " ms"
                << " | Total Frames: " << stats.totalFrames;
            glfwSetWindowTitle(m_window->getHandle(), title.str().c_str());

            m_lastTitleUpdate = now;
        }
    }

    float FrameMonitor::getTime() const {
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> elapsed = now - m_startTime;
        return elapsed.count();
    }
}