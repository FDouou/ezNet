#pragma once

#include <string>

namespace ezNet {

class Logger {
public:
    enum class Level {
        TRACE,
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL
    };

    static Logger& instance();

    void setLevel(Level level);
    Level level() const;

    void log(Level level, const char* file, int line, const char* fmt, ...);

private:
    Logger();
    Level level_;
};

#define LOG_TRACE(fmt, ...) Logger::instance().log(Logger::Level::TRACE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) Logger::instance().log(Logger::Level::DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  Logger::instance().log(Logger::Level::INFO,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  Logger::instance().log(Logger::Level::WARN,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) Logger::instance().log(Logger::Level::ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) Logger::instance().log(Logger::Level::FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

} // namespace ezNet
