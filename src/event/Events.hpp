#pragma once 
#include "EventDispatcher.hpp"
#include "events/DebugMessageEvent.hpp"
#include "events/KeyEvent.hpp"
#include "events/MouseButtonEvent.hpp"
#include "events/WindowResizeEvent.hpp"
#include "events/MouseScrollEvent.hpp"
#include "events/MouseMoveEvent.hpp"
#include "events/CameraSwitchEvent.hpp"

namespace StarryEngine {
    inline EventDispatcher& GetEventDispatcher() {
        return EventDispatcher::getInstance();
    }
}