#pragma once
#include "../IEvent.hpp"

namespace StarryEngine {

    class WindowResizeEvent : public IEvent {
    public:
        WindowResizeEvent(uint32_t width, uint32_t height)
            : m_width(width), m_height(height) {
        }

        EventType getType() const override { return EventType::WindowResize; }

        uint32_t getWidth() const { return m_width; }
        uint32_t getHeight() const { return m_height; }

    private:
        uint32_t m_width, m_height;
    };

} // namespace StarryEngine