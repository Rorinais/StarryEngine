#pragma once
#include "EventType.hpp"

namespace StarryEngine {

    class IEvent {
    public:
        virtual ~IEvent() = default;
        virtual EventType getType() const = 0;
    };

} // namespace StarryEngine