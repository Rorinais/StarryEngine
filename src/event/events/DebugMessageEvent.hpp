#pragma once
#include "../IEvent.hpp"
#include "../../renderer/interface/RHI_ENUMS.hpp"
#include <string>

namespace StarryEngine {

    class DebugMessageEvent : public IEvent {
    public:
        DebugMessageEvent(RHI::MessageSeverity severity, RHI::MessageSource source, std::string message)
            : m_severity(severity), m_source(source), m_message(std::move(message)) {
        }

        EventType getType() const override { return EventType::DebugMessage; }

        RHI::MessageSeverity getSeverity() const { return m_severity; }
        RHI::MessageSource getSource() const { return m_source; }
        const std::string& getMessage() const { return m_message; }

    private:
        RHI::MessageSeverity m_severity;
        RHI::MessageSource m_source;
        std::string m_message;
    };

} // namespace StarryEngine