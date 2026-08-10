#include <event/EventDispatcher.hpp>

namespace StarryEngine {
    size_t EventDispatcher::subscribe(EventType type, EventHandler handler) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_listeners[type].push_back({ ++m_nextId, std::move(handler) });
        return m_nextId;
    }

    void EventDispatcher::unsubscribe(EventType type, size_t listenerId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_listeners.find(type);
        if (it != m_listeners.end()) {
            auto& listeners = it->second;
            listeners.erase(
                std::remove_if(listeners.begin(), listeners.end(),
                    [listenerId](const Listener& l) { return l.id == listenerId; }),
                listeners.end()
            );
        }
    }

    void EventDispatcher::dispatch(IEvent& event) {
        std::vector<Listener> listenersCopy;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_listeners.find(event.getType());
            if (it != m_listeners.end()) {
                listenersCopy = it->second;
            }
        }
        for (auto& listener : listenersCopy) {
            listener.handler(event);
        }
    }
}