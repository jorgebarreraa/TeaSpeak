#include "LogUtils.h"
#include "LogSinks.h"
#include <iomanip>
#include <fstream>
#include <map>
#ifdef WIN32
    #include <filesystem>
    namespace fs = std::filesystem;
#else
    #include <experimental/filesystem>
    namespace fs = std::experimental::filesystem;
#endif
#include <StringVariable.h>
#include <mutex>

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>

#ifdef HAVE_CXX_TERMINAL
    #include <CXXTerminal/Terminal.h>
#endif

using namespace std;
using namespace std::chrono;
using namespace spdlog;

#define ASYNC_LOG
namespace logger {
    recursive_mutex loggerLock;
    map<size_t, std::shared_ptr<spdlog::logger>> loggers;
    shared_ptr<LoggerConfig> logConfig;
    shared_ptr<::logger::TerminalSink> terminalSink;

    std::shared_ptr<spdlog::details::thread_pool> logging_threads{nullptr};

    spdlog::level::level_enum min_level{spdlog::level::trace};

    void updater_logger_levels(const std::shared_ptr<spdlog::logger>& logger) {
        for(const auto& sink : logger->sinks())
            if(dynamic_pointer_cast<TerminalSink>(sink)) {
                sink->set_level(::logger::currentConfig()->terminalLevel);
            } else if(dynamic_pointer_cast<spdlog::sinks::rotating_file_sink_mt>(sink)) {
                sink->set_level(::logger::currentConfig()->logfileLevel);
            } else if(dynamic_pointer_cast<spdlog::sinks::rotating_file_sink_st>(sink)) {
                sink->set_level(::logger::currentConfig()->logfileLevel);
            } else {
                sink->set_level(min_level);
            }
        logger->set_level(min_level);
    }

    std::string generate_log_file(size_t group) {
        return strvar::transform(logConfig->logPath,
                                 strvar::StringValue{"group", group != -1 ? to_string(group) : "general"},
                                 strvar::FunctionValue("time", (strvar::FunctionValue::FValueFNEasy) [](std::deque<std::string> value) -> std::string {
                                     auto pattern = !value.empty() ? value[0] : "%Y-%m-%d_%H:%M:%S";

                                     auto secs = duration_cast<seconds>(logConfig->timestamp.time_since_epoch()).count();
                                     tm* tm_info;
#ifdef WIN32
                                     tm _tm_info{};
                                     localtime_s(&_tm_info, &secs);
                                     tm_info = &_tm_info;
#else
                                     tm_info = localtime((time_t*) &secs);
#endif
                                     char timeBuffer[1024];
                                     if(strftime(timeBuffer, 1024, pattern.c_str(), tm_info) == 0) {
                                         return string("string is longer than the buffer");
                                     }

                                     return string(timeBuffer);
                                 })
        );
    }

    std::mutex default_lock{};
    bool default_setup{false};

    std::shared_ptr<spdlog::logger> default_logger() {
        lock_guard lock{default_lock};
        if(!default_setup) {
            default_setup = true;

            spdlog::default_logger()->sinks().clear();
            auto terminal_sink = make_shared<TerminalSink>();
            terminal_sink->set_level(spdlog::level::trace);
            spdlog::default_logger()->sinks().push_back(terminal_sink);

            spdlog::default_logger()->set_formatter(std::make_unique<LogFormatter>(true));
        }
        return spdlog::default_logger();
    }

    shared_ptr<spdlog::logger> logger(int serverId) {
        if(!::logger::currentConfig())
            return default_logger();

        size_t group = 0;
        if(::logger::currentConfig()->vs_group_size > 0 && serverId > 0)
            group = serverId / ::logger::currentConfig()->vs_group_size;
        else group = -1;

        if(loggers.count(group) == 0) {
            lock_guard lock(loggerLock);
            if(loggers.count(group) > 0) return loggers[group];
            //Create a new logger
            if(group != 0 && group != -1)
                logger(0)->debug("Creating new grouped logger for group {}", group);

            vector<spdlog::sink_ptr> sinks;
            string path;
            if(logConfig->logfileLevel != spdlog::level::off) {
                path = generate_log_file(group);

                auto logFile = fs::u8path(path);
                if(!logFile.parent_path().empty())
                    fs::create_directories(logFile.parent_path());

                try {
                    auto sink = make_shared<spdlog::sinks::rotating_file_sink_mt>(logFile.string(), 1024 * 1024 * 50, 12);
                    sink->set_formatter(std::make_unique<LogFormatter>(::logger::currentConfig()->file_colored));
                    sinks.push_back(sink);
                } catch(std::exception& ex) {
                    if(group != 0 && group != -1)
                        logger(0)->critical("Failed to create file for new log group: {}", ex.what());
                    else
#ifdef HAVE_CXX_TERMINAL
                        terminal::instance()->writeMessage("§4[CRITICAL] §eFailed to create main log file: " + string{ex.what()}, false);
#else
                        std::cout << "[CRITICAL] Failed to create main log file: " << ex.what() << "\n";
#endif
                }
            } else {
                path = "/dev/null (" + to_string(group) + ")";
            }
            sinks.push_back(terminalSink);

            if(!logging_threads && !logConfig->sync)
                logging_threads = std::make_shared<spdlog::details::thread_pool>(8192, 1); //Only one thread possible here, else elements get reordered

            std::shared_ptr<spdlog::logger> logger;
            if(!logConfig->sync) {
                logger = std::make_shared<spdlog::async_logger>("Logger (" + path + ")", sinks.begin(), sinks.end(), logging_threads, async_overflow_policy::block);
            } else {
                logger = std::make_shared<spdlog::logger>("Logger (" + path + ")", sinks.begin(), sinks.end());
                logger->flush_on(level::trace);
            }

            updater_logger_levels(logger);
            loggers[group] = logger;
        }

        return loggers[group];
    }

    const std::shared_ptr<LoggerConfig>& currentConfig() {
        return logConfig;
    }

    extern void setup(const shared_ptr<LoggerConfig>& config) {
        logConfig = config;
        config->timestamp = system_clock::now();

        terminalSink = make_shared<TerminalSink>();
        terminalSink->set_level(::logger::currentConfig()->terminalLevel);
        terminalSink->set_formatter(std::make_unique<LogFormatter>(true));
        min_level = ::min(::logger::currentConfig()->logfileLevel, ::logger::currentConfig()->terminalLevel);

        logger(0)->debug("Log successfully started!");
    }


    bool should_log(spdlog::level::level_enum level) {
        return level >= min_level;
    }

    void log(logger::forceable level, int server_id, const std::string_view& buffer) {
        auto logger = ::logger::logger(server_id);

        auto message_format = "§8{0:>5} | §r{1}";
        if(server_id <= 0) {
            switch (server_id) {
                case LOG_INSTANCE:
                    message_format = "§8GLOBL | §r{1}";
                    break;
                case LOG_QUERY:
                    message_format = "§8QUERY | §r{1}";
                    break;
                case LOG_FT:
                    message_format = "§8 FILE | §r{1}";
                    break;
                case LOG_GENERAL:
                    message_format = "§8  GEN | §r{1}";
                    break;
                case LOG_LICENSE_CONTROLL:
                    message_format = "§8  CONTR | §r{1}";
                    break;
                case LOG_LICENSE_WEB:
                    message_format = "§8  WEBST | §r{1}";
                    break;

                default:
                    break;
            }
        }

        try {
            logger->log(level.level, message_format, server_id, buffer);
        } catch (const std::exception &ex) {
            //TODO better?
            std::cerr << "An exception has raised while logging a message (" << ex.what() << "): " << buffer << "\n";
        } catch(...) {
            std::cerr << "An unknown exception has raised while logging a message: " << buffer << "\n";
            throw;
        }
    }

    void updateLogLevels() {
        lock_guard lock(loggerLock);
        min_level = ::min(::logger::currentConfig()->logfileLevel, ::logger::currentConfig()->terminalLevel);
        for(const auto& logger : loggers) {
            updater_logger_levels(logger.second);
        }
    }

    void flush() {
        unique_lock lock(loggerLock);
        auto _loggers = loggers;
        lock.unlock();

        for(const auto& loggerEntry : _loggers) {
            loggerEntry.second->flush();
        }
    }

    extern void uninstall() {
        lock_guard lock(loggerLock);
        for(auto& loggerEntry : loggers) {
            loggerEntry.second->flush();
            loggerEntry.second.reset();
        }
        loggers.clear();
        spdlog::drop_all();

        logConfig = nullptr;
        terminalSink = nullptr;
    }
}

void hexDump(void *addr, int len, int pad,int columnLength, void (*print)(string));
void hexDump(void *addr, int len, int pad,int columnLength) {
    hexDump(addr, len, pad, columnLength, [](string str){ logMessage(0, "\n{}", str); });
}

void hexDump(void *addr, int len, int pad,int columnLength, void (*print)(string)) {
    int i;
    uint8_t* buff = new uint8_t[pad+1];
    unsigned char* pc = (unsigned char*)addr;

    if (len <= 0) {
        return;
    }

    stringstream line;
    line << uppercase << hex << setfill('0');
    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % pad) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0) {
                line << buff;
                print(line.str());
                line = stringstream{};
                line << hex;
            };

            // Output the offset.
            line << setw(4) << i << "    ";
        }
        if(i % columnLength == 0 && i % pad != 0){
            line << "| ";
        }

        // Now the hex code for the specific character.
        line << setw(2) << (int) pc[i] << " ";

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % pad] = '.';
        else
            buff[i % pad] = pc[i];
        buff[(i % pad) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % pad) != 0) {
        line << "   ";
        i++;
    }

    line << buff;
    delete[] buff;

    print(line.str());
    line = stringstream{};
    line << "Length: " << dec << len << " Addr: " << addr;
    print(line.str());
}
