#pragma once 
#include <event/EventDispatcher.hpp>
#include <event/events/DebugMessageEvent.hpp>
#include <event/events/KeyEvent.hpp>
#include <event/events/MouseButtonEvent.hpp>
#include <event/events/WindowResizeEvent.hpp>
#include <event/events/MouseScrollEvent.hpp>
#include <event/events/MouseMoveEvent.hpp>
#include <event/events/CameraSwitchEvent.hpp>

namespace StarryEngine {
    inline EventDispatcher& GetEventDispatcher() {
        return EventDispatcher::getInstance();
    }
}