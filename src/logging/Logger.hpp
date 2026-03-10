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

    class ImGuiLogSink : public spdlog::sinks::base_sink<std::mutex> {
    public:
        std::vector<std::string> getLogs() {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_logs;  // 返回副本
        }

        void clear() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_logs.clear();
        }

    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override {
            spdlog::memory_buf_t formatted;
            formatter_->format(msg, formatted);
            std::string log_str = fmt::to_string(formatted);

            std::lock_guard<std::mutex> lock(m_mutex);
            m_logs.push_back(log_str);
            if (m_logs.size() > 1000) {
                m_logs.erase(m_logs.begin());
            }
        }

        void flush_() override {}

    private:
        std::vector<std::string> m_logs;
        std::mutex m_mutex;
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
        static void updatePatterns(); // 更新所有 sink 的格式

        static std::shared_ptr<spdlog::logger> s_logger;
        static std::shared_ptr<ImGuiLogSink> s_imguiSink;
        static bool s_showSourceLoc; // 新增静态成员
    };

} // namespace StarryEngine

// 带源文件位置的新日志宏
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