#pragma once
#include <spdlog/spdlog.h>  
#include <spdlog/sinks/base_sink.h>
#include <spdlog/details/log_msg.h>
#include <memory>
#include <string>
#include <vector>
#include <mutex>

#include "Formatters.hpp"

namespace StarryEngine {
    struct LogEntry {
        spdlog::level::level_enum level;
        std::string message;
    };

    class ImGuiLogSink : public spdlog::sinks::base_sink<std::mutex> {
    public:
        // 线程安全地返回日志拷贝
        std::vector<LogEntry> getLogs() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_logs;
        }

        void clear() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_logs.clear();
        }

    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override {
            spdlog::memory_buf_t formatted;
            formatter_->format(msg, formatted);
            std::string message = fmt::to_string(formatted);

            std::lock_guard<std::mutex> lock(m_mutex);
            m_logs.push_back({ msg.level, std::move(message) });
            if (m_logs.size() > 1000) {
                m_logs.erase(m_logs.begin());
            }
        }

        void flush_() override {}

    private:
        std::vector<LogEntry> m_logs;          // 改为 LogEntry
        mutable std::mutex m_mutex;            // 保护 m_logs 的锁
    };

    class Logger {
    public:
        static void init(const std::string& logFile = "", bool flushOnInfo = true);
        static std::shared_ptr<spdlog::logger> get();
        static void setLevel(const std::string& level);
        static void setLevel(spdlog::level::level_enum level);
        static void shutdown();
        static std::shared_ptr<ImGuiLogSink> getImGuiSink();

        static void setShowSourceLoc(bool enable);
        static bool getShowSourceLoc();

    private:
        Logger() = delete;
        static void updatePatterns();

        static std::shared_ptr<spdlog::logger> s_logger;
        static std::shared_ptr<ImGuiLogSink> s_imguiSink;
        static bool s_showSourceLoc;
    };

} // namespace StarryEngine

#define LOG_TRACE(...)    do { \
    if (auto logger = StarryEngine::Logger::get()) \
        logger->log(spdlog::source_loc{__FILE__, __LINE__, __FUNCTION__}, \
                    spdlog::level::trace, __VA_ARGS__); \
} while(0)

#define LOG_DEBUG(...)    do { \
    if (auto logger = StarryEngine::Logger::get()) \
        logger->log(spdlog::source_loc{__FILE__, __LINE__, __FUNCTION__}, \
                    spdlog::level::debug, __VA_ARGS__); \
} while(0)

#define LOG_INFO(...)     do { \
    if (auto logger = StarryEngine::Logger::get()) \
        logger->log(spdlog::source_loc{__FILE__, __LINE__, __FUNCTION__}, \
                    spdlog::level::info, __VA_ARGS__); \
} while(0)

#define LOG_WARN(...)     do { \
    if (auto logger = StarryEngine::Logger::get()) \
        logger->log(spdlog::source_loc{__FILE__, __LINE__, __FUNCTION__}, \
                    spdlog::level::warn, __VA_ARGS__); \
} while(0)

#define LOG_ERROR(...)    do { \
    if (auto logger = StarryEngine::Logger::get()) \
        logger->log(spdlog::source_loc{__FILE__, __LINE__, __FUNCTION__}, \
                    spdlog::level::err, __VA_ARGS__); \
} while(0)

#define LOG_CRITICAL(...) do { \
    if (auto logger = StarryEngine::Logger::get()) \
        logger->log(spdlog::source_loc{__FILE__, __LINE__, __FUNCTION__}, \
                    spdlog::level::critical, __VA_ARGS__); \
} while(0)