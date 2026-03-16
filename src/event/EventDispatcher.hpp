#pragma once
#include "IEvent.hpp"
#include <functional>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>
#include <algorithm>

namespace StarryEngine {

    class EventDispatcher {
    public:
        using EventHandler = std::function<void(IEvent&)>;

        static EventDispatcher& getInstance() {
            static EventDispatcher instance;
            return instance;
        }

        // 订阅事件
        size_t subscribe(EventType type, EventHandler handler);

        // 取消订阅
        void unsubscribe(EventType type, size_t listenerId);

        // 分发事件
        void dispatch(IEvent& event);

        template<typename T, typename... Args>
        void dispatch(Args&&... args) {
            T event(std::forward<Args>(args)...);
            dispatch(static_cast<IEvent&>(event));
        }

    private:
        EventDispatcher() = default;

        struct Listener {
            size_t id;
            EventHandler handler;
        };

        std::unordered_map<EventType, std::vector<Listener>> m_listeners;
        size_t m_nextId = 0;
        std::mutex m_mutex;
    };

} // namespace StarryEngine