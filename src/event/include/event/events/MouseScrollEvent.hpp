#pragma once
#include <event/IEvent.hpp>

namespace StarryEngine {

    class MouseScrollEvent : public IEvent {
    public:
        MouseScrollEvent(double xOffset, double yOffset)
            : m_xOffset(xOffset), m_yOffset(yOffset) {
        }

        EventType getType() const override { return EventType::MouseScrolled; }

        double getXOffset() const { return m_xOffset; }
        double getYOffset() const { return m_yOffset; }

    private:
        double m_xOffset, m_yOffset;
    };

} // namespace StarryEngine