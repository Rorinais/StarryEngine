#include"EventDispatcher.hpp"

namespace StarryEngine {
    size_t EventDispatcher::subscribe(EventType type, EventHandler handler) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_listeners[type].push_back({ ++m_nextId, std::move(handler) });
        return m_nextId;
    }

    // 取消订阅
    void EventDispatcher::unsubscribe(EventType type, size_t listenerId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& listeners = m_listeners[type];
        listeners.erase(
            std::remove_if(listeners.begin(), listeners.end(),
                [listenerId](const Listener& l) { return l.id == listenerId; }),
            listeners.end()
        );
    }

    // 分发事件
    void EventDispatcher::dispatch(IEvent& event) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_listeners.find(event.getType());
        if (it != m_listeners.end()) {
            // 复制一份监听器列表，避免在遍历过程中修改
            auto listenersCopy = it->second;
            for (auto& listener : listenersCopy) {
                listener.handler(event);
            }
        }
    }

}