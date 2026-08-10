#pragma once
#include <event/IEvent.hpp>

namespace StarryEngine {

    class MouseButtonEvent : public IEvent {
    public:
        MouseButtonEvent(int button, int action, int mods)
            : m_button(button), m_action(action), m_mods(mods) {
        }

        EventType getType() const override { return EventType::MouseButtonPressed; }
        int getButton() const { return m_button; }
        int getAction() const { return m_action; }
        int getMods() const { return m_mods; }

    private:
        int m_button, m_action, m_mods;
    };

} // namespace StarryEngine