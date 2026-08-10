#pragma once
#include <event/IEvent.hpp>

namespace StarryEngine {

    class MouseMoveEvent : public IEvent {
    public:
        MouseMoveEvent(double x, double y)
            : m_x(x), m_y(y) {
        }

        EventType getType() const override { return EventType::MouseMoved; }

        double getX() const { return m_x; }
        double getY() const { return m_y; }

    private:
        double m_x, m_y;
    };

} // namespace StarryEngine