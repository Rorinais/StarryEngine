#include "Logger.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <iostream>

namespace StarryEngine {

    std::shared_ptr<spdlog::logger> Logger::s_logger = nullptr;
    std::shared_ptr<ImGuiLogSink> Logger::s_imguiSink = nullptr;
    bool Logger::s_showSourceLoc = true; // 默认开启

    void Logger::init(const std::string& logFile, bool flushOnInfo) {
        if (s_logger) {
            shutdown();
        }

        try {
            std::vector<spdlog::sink_ptr> sinks;

            // 创建控制台 sink（不设固定 pattern，稍后统一更新）
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            sinks.push_back(console_sink);

            // 文件 sink（如果启用）
            if (!logFile.empty()) {
                auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFile, true);
                sinks.push_back(file_sink);
            }

            // ImGui sink
            auto imguiSink = std::make_shared<ImGuiLogSink>();
            sinks.push_back(imguiSink);
            s_imguiSink = imguiSink;

            // 创建 logger
            s_logger = std::make_shared<spdlog::logger>("StarryEngine", sinks.begin(), sinks.end());
            spdlog::register_logger(s_logger);
            spdlog::set_default_logger(s_logger);

            // 设置日志级别
#ifdef NDEBUG
            s_logger->set_level(spdlog::level::info);
#else
            s_logger->set_level(spdlog::level::trace);
#endif

            if (flushOnInfo) {
                s_logger->flush_on(spdlog::level::info);
            }

            // 统一更新所有 sink 的格式（根据 s_showSourceLoc）
            updatePatterns();

            s_logger->log(spdlog::source_loc{ __FILE__, __LINE__, __FUNCTION__ },
                spdlog::level::info, "Logger initialized.");
        }
        catch (const spdlog::spdlog_ex& ex) {
            std::cerr << "Logger initialization failed: " << ex.what() << std::endl;
        }
    }

    void Logger::updatePatterns() {
        if (!s_logger) return;

        // 基础格式
        std::string basePattern = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$]";
        // 根据开关决定是否添加源位置
        std::string sourcePart = s_showSourceLoc ? " [%s:%#]" : "";
        std::string fullPattern = basePattern + sourcePart + " %v";

        // 应用到所有 sink
        for (auto& sink : s_logger->sinks()) {
            sink->set_pattern(fullPattern);
        }
    }

    void Logger::setShowSourceLoc(bool enable) {
        if (s_showSourceLoc == enable) return;
        s_showSourceLoc = enable;
        updatePatterns();
    }

    bool Logger::getShowSourceLoc() {
        return s_showSourceLoc;
    }

    std::shared_ptr<ImGuiLogSink> Logger::getImGuiSink() {
        return s_imguiSink;
    }

    std::shared_ptr<spdlog::logger> Logger::get() {
        return s_logger;
    }

    void Logger::setLevel(const std::string& level) {
        if (!s_logger) return;
        if (level == "trace") s_logger->set_level(spdlog::level::trace);
        else if (level == "debug") s_logger->set_level(spdlog::level::debug);
        else if (level == "info") s_logger->set_level(spdlog::level::info);
        else if (level == "warn") s_logger->set_level(spdlog::level::warn);
        else if (level == "error") s_logger->set_level(spdlog::level::err);
        else if (level == "critical") s_logger->set_level(spdlog::level::critical);
        else s_logger->warn("Unknown log level: {}", level);
    }

    void Logger::setLevel(spdlog::level::level_enum level) {
        if (s_logger) s_logger->set_level(level);
    }

    void Logger::shutdown() {
        if (s_logger) {
            s_logger->log(spdlog::source_loc{ __FILE__, __LINE__, __FUNCTION__ },
                spdlog::level::info, "Logger shutting down.");
            spdlog::drop_all(); // 释放所有 logger
            s_logger.reset();
        }
    }

} // namespace StarryEngine