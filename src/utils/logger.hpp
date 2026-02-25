#pragma once

#include <format>
#include <functional>
#include <string>

#include "mslogin_export.hpp"

/**
 * @brief 日志级别枚举
 *
 * 定义了不同的日志级别，用于区分日志的重要程度
 */
enum class LogLevel {
    Debug    = 0, // 调试信息
    Info     = 1, // 信息
    Warning  = 2, // 警告
    Error    = 3, // 错误
    Critical = 4  // 严重错误
};

using LogCallback = std::function<void(LogLevel level, const std::string& message)>;

class MSLOGIN_API Logger {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    // 设置日志回调函数
    void setLogCallback(LogCallback callback) { mLogCallback = callback; }

    // 清除日志回调函数
    void clearLogCallback() { mLogCallback = nullptr; }

    // 记录日志消息
    void log(LogLevel level, const std::string& message) {
        if (mLogCallback) {
            mLogCallback(level, message);
        }
    }

    // 便利方法
    void debug(const std::string& message) { log(LogLevel::Debug, message); }

    void info(const std::string& message) { log(LogLevel::Info, message); }

    void warning(const std::string& message) { log(LogLevel::Warning, message); }

    void error(const std::string& message) { log(LogLevel::Error, message); }

    void critical(const std::string& message) { log(LogLevel::Critical, message); }

private:
    Logger() { mLogCallback = nullptr; }
    ~Logger() = default;

    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;

    LogCallback mLogCallback;
};

// 格式化辅助函数（处理临时值参数）
namespace detail {
template <typename... Args> std::string format_log(std::string_view fmt, Args&&... args) { return std::vformat(fmt, std::make_format_args(args...)); }
} // namespace detail

// 便利宏定义（支持 std::format 格式化参数）
#define LOG_DEBUG(fmt, ...) Logger::getInstance().debug(detail::format_log(fmt __VA_OPT__(, ) __VA_ARGS__))
#define LOG_INFO(fmt, ...) Logger::getInstance().info(detail::format_log(fmt __VA_OPT__(, ) __VA_ARGS__))
#define LOG_WARNING(fmt, ...) Logger::getInstance().warning(detail::format_log(fmt __VA_OPT__(, ) __VA_ARGS__))
#define LOG_ERROR(fmt, ...) Logger::getInstance().error(detail::format_log(fmt __VA_OPT__(, ) __VA_ARGS__))
#define LOG_CRITICAL(fmt, ...) Logger::getInstance().critical(detail::format_log(fmt __VA_OPT__(, ) __VA_ARGS__))
