#pragma once

#include <functional>
#include <string>

enum class LogLevel {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Critical = 4
};

class Logger {
public:
    using LogCallback = std::function<void(LogLevel level, const std::string& message)>;

    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    // 设置日志回调
    void setLogCallback(LogCallback callback) {
        mLogCallback = callback;
    }

    // 清除日志回调
    void clearLogCallback() {
        mLogCallback = nullptr;
    }

    // 记录日志
    void log(LogLevel level, const std::string& message) {
        if (mLogCallback) {
            mLogCallback(level, message);
        }
    }

    // 便利方法
    void debug(const std::string& message) {
        log(LogLevel::Debug, message);
    }

    void info(const std::string& message) {
        log(LogLevel::Info, message);
    }

    void warning(const std::string& message) {
        log(LogLevel::Warning, message);
    }

    void error(const std::string& message) {
        log(LogLevel::Error, message);
    }

    void critical(const std::string& message) {
        log(LogLevel::Critical, message);
    }

private:
    Logger() : mLogCallback(nullptr) {}
    ~Logger() = default;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    LogCallback mLogCallback;
};

// 便利宏定义
#define LOG_DEBUG(msg) Logger::getInstance().debug(msg)
#define LOG_INFO(msg) Logger::getInstance().info(msg)
#define LOG_WARNING(msg) Logger::getInstance().warning(msg)
#define LOG_ERROR(msg) Logger::getInstance().error(msg)
#define LOG_CRITICAL(msg) Logger::getInstance().critical(msg)
