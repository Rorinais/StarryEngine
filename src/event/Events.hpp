#pragma once 
#include "EventDispatcher.hpp"
#include "events/DebugMessageEvent.hpp"
#include "events/KeyEvent.hpp"
#include "events/MouseButtonEvent.hpp"
#include "events/WindowResizeEvent.hpp"

namespace StarryEngine {
    inline EventDispatcher& GetEventDispatcher() {
        return EventDispatcher::getInstance();
    }
}