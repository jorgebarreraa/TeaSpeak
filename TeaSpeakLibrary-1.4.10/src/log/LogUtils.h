#pragma once

#define SPDLOG_EOL "\n"
#ifdef SPDLOG_FINAL
    #undef SPDLOG_FINAL
#endif
#define SPDLOG_FINAL
#ifdef byte
    #undef byte
#endif

#include <spdlog/logger.h>
#include <spdlog/fmt/fmt.h>
#include <sstream>
#include <string>
#include <chrono>
#include "../Definitions.h"

#ifdef HAVE_CXX_TERMINAL
    #include <CXXTerminal/Terminal.h>
#endif

#ifdef log
#undef log
#endif
namespace logger {
    struct LoggerConfig {
        spdlog::level::level_enum logfileLevel = spdlog::level::info;
        spdlog::level::level_enum terminalLevel = spdlog::level::info;
        bool sync{false};
        bool file_colored;
        std::string logPath = "log_${time}.log";
        size_t vs_group_size = 1;
        std::chrono::system_clock::time_point timestamp;
    };

    extern std::shared_ptr<spdlog::logger> logger(int);
    extern void setup(const std::shared_ptr<LoggerConfig>&);
    extern const std::shared_ptr<LoggerConfig>& currentConfig();
    extern void uninstall();

    extern bool should_log(spdlog::level::level_enum /* level */); //TODO: inline?
    extern void log(spdlog::level::forceable /* level */, int /* server id */, const std::string_view& /* buffer */);

    extern void updateLogLevels();
    extern void flush();

    namespace impl {
        template <spdlog::level::level_enum level, typename... Args>
        inline void do_log(bool forced, int serverId, const std::string& message, const Args&... args) {
            if(!forced && !::logger::should_log(level)) return;
            spdlog::memory_buf_t buffer{};

            auto _logger = ::logger::logger(serverId);
            std::string fmt_message;
            try {
                fmt_message = fmt::format(message, args...);
            } catch (const std::exception &ex) {
                fmt_message = "failed to format message '" + std::string{message} + "': " + ex.what();
            }

            ::logger::log(spdlog::level::forceable{level, forced}, serverId, fmt_message);
        }
    }
}

#define LOG_LICENSE_CONTROLL    (-0x10)
#define LOG_LICENSE_WEB         (-0x11)

#define LOG_INSTANCE (-1)
#define LOG_QUERY    (-2)
#define LOG_FT       (-3)
#define LOG_GENERAL   0

#define DEFINE_LOG_IMPL(name, level, _default_prefix) \
template <typename... Args> \
inline void name ##Fmt(bool forced, int serverId, const std::string& message, const Args&... args) { \
    ::logger::impl::do_log<level>(forced, serverId, message, args...); \
}

DEFINE_LOG_IMPL(logMessage,   spdlog::level::info,      "INFO")
DEFINE_LOG_IMPL(logError,     spdlog::level::err,       "ERROR")
DEFINE_LOG_IMPL(logWarning,   spdlog::level::warn,      "WARNING")
DEFINE_LOG_IMPL(logCritical,  spdlog::level::critical,  "CRITICAL")
DEFINE_LOG_IMPL(logTrace,     spdlog::level::trace,     "TRACE")
DEFINE_LOG_IMPL(debugMessage, spdlog::level::debug,     "DEBUG")

#define LOG_METHOD(name)                                                                                                    \
template <typename... Args> \
inline void name(int serverId, const std::string& message, const Args&... args){ name ##Fmt(false, serverId, message, args...); }        \
inline void name(int serverId, const std::string& message){ name ##Fmt(false, serverId, message); }        \
inline void name(int serverId, std::ostream& str) = delete;        \
inline void name(const std::string& message) = delete;                                                          \
inline void name(std::ostream& str) = delete;                                \
inline void name(bool, int, const std::string&) = delete;

LOG_METHOD(logError);
LOG_METHOD(logWarning);
LOG_METHOD(logMessage);
LOG_METHOD(logCritical);
LOG_METHOD(logTrace);
LOG_METHOD(debugMessage);

#undef LOG_METHOD