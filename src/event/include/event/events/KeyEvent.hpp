#pragma once
#include <event/IEvent.hpp>

namespace StarryEngine {

    class KeyEvent : public IEvent {
    public:
        KeyEvent(int key, int scancode, int action, int mods)
            : m_key(key), m_scancode(scancode), m_action(action), m_mods(mods) {
        }

        EventType getType() const override { return EventType::KeyPressed; } // 可根据 action 区分不同事件，或统一用一个事件再判断

        int getKey() const { return m_key; }
        int getScancode() const { return m_scancode; }
        int getAction() const { return m_action; }
        int getMods() const { return m_mods; }

    private:
        int m_key, m_scancode, m_action, m_mods;
    };

} // namespace StarryEngine