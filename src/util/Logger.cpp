#include "util/Logger.h"
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <mutex>

namespace ezNet {

static std::mutex g_logMutex;

Logger::Logger() : level_(Level::INFO) {}

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::setLevel(Level level) {
    level_ = level;
}

Logger::Level Logger::level() const {
    return level_;
}

void Logger::log(Level level, const char* file, int line, const char* fmt, ...) {
    if (level < level_) return;

    std::lock_guard<std::mutex> lock(g_logMutex);

    time_t now = time(nullptr);
    struct tm tmBuf;
    localtime_r(&now, &tmBuf);
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tmBuf);

    const char* levelStr = "UNKNOWN";
    switch (level) {
        case Level::TRACE: levelStr = "TRACE"; break;
        case Level::DEBUG: levelStr = "DEBUG"; break;
        case Level::INFO:  levelStr = "INFO";  break;
        case Level::WARN:  levelStr = "WARN";  break;
        case Level::ERROR: levelStr = "ERROR"; break;
        case Level::FATAL: levelStr = "FATAL"; break;
    }

    char msgBuf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msgBuf, sizeof(msgBuf), fmt, args);
    va_end(args);

    FILE* out = (level >= Level::WARN) ? stderr : stdout;
    fprintf(out, "[%s] [%s] [%s:%d] %s\n", timeStr, levelStr, file, line, msgBuf);
}

} // namespace ezNet
