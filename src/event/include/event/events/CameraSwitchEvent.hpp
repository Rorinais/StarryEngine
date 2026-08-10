#pragma once
#include <event/IEvent.hpp>

namespace StarryEngine {

    class CameraSwitchEvent : public IEvent {
    public:
        CameraSwitchEvent(uint32_t cameraIndex)
            : m_cameraIndex(cameraIndex) {
        }

        EventType getType() const override { return EventType::CameraSwitch; }

        uint32_t getCameraIndex() const { return m_cameraIndex; }

    private:
        uint32_t m_cameraIndex;
    };

} // namespace StarryEngine