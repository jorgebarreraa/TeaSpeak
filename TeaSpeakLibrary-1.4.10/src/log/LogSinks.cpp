#include "LogUtils.h"
#include "LogSinks.h"
#include <ctime>
#include <array>

using namespace std;
using namespace spdlog;

namespace logger {
    void TerminalSink::sink_it_(const spdlog::details::log_msg &msg) {
        memory_buf_t formatted;
        this->formatter_->format(msg, formatted);

        std::string_view message{formatted.data(), formatted.size()};

#ifdef HAVE_CXX_TERMINAL
        if (terminal::active()) {
            //Split the string at new lines
            size_t index{0}, found;
            do {
                found = message.find('\n', index);
                const auto length = (found == -1 ? message.length() : found) - index;
                const auto line = message.substr(index, length);

                index = found;
                if(length == 0) {
                    continue;
                }

                terminal::instance()->writeMessage(std::string{line});
            } while(++index);
        } else {
            cout << message << std::flush;
        }
#else
        cout << message << std::flush;
#endif
    }

    void TerminalSink::flush_() {
#ifdef HAVE_CXX_TERMINAL
        if(!terminal::active())
#endif
        std::cout.flush();
    }


    inline void append_time(const log_clock::time_point& point, memory_buf_t& dest) {
        std::time_t time = log_clock::to_time_t(point);
        std::tm timetm = *std::localtime(&time);

        static constexpr auto max_length = 20;
        dest.reserve(dest.size() + max_length);

        auto length = strftime(dest.end(), max_length, "%Y-%m-%d %H:%M:%S", &timetm);
        if(length < 0)
            length = 0;

        dest.resize(dest.size() + length);
    }

#ifdef HAVE_CXX_TERMINAL
    static constexpr std::array<std::string_view, spdlog::level::off + 1> level_mapping_colored{
            " [" ANSI_LIGHT_BLUE                 "TRACE" ANSI_RESET "] ",
            " [" ANSI_LIGHT_BLUE                 "DEBUG" ANSI_RESET "] ",
            " [" ANSI_YELLOW                     "INFO " ANSI_RESET "] ",
            " [" ANSI_BROWN                      "WARNING " ANSI_RESET "] ",
            " [" ANSI_RED                        "ERROR" ANSI_RESET "] ",
            " [" ANSI_RED ANSI_BOLD ANSI_REVERSE "CRITICAL" ANSI_RESET "] ",
            " [" ANSI_GRAY                       "OFF     " ANSI_RESET "] "
    };
#endif

    static constexpr std::array<std::string_view, spdlog::level::off + 1> level_mapping{
            " [TRACE] ",
            " [DEBUG] ",
            " [INFO ] ",
            " [WARNING ] ",
            " [ERROR] ",
            " [CRITICAL] ",
            " [OFF  ] "
    };

    void LogFormatter::format(const details::log_msg &msg, memory_buf_t &dest) {
        const auto append = [&](const std::string_view& message) { dest.append(message.data(), message.data() + message.length()); };

        dest.clear();
        auto prefix_begin = dest.end();
        //Time
        {
            dest.push_back('[');
            append_time(msg.time, dest);
            dest.push_back(']');
        }

        //Level
        {
#ifdef HAVE_CXX_TERMINAL
            const auto& mapping = this->_colored ? level_mapping_colored : level_mapping;
#else
            const auto& mapping = level_mapping;
#endif
            size_t level = msg.level.value;
            if(level >= mapping.size()) {
                level = mapping.size() - 1;
            }

            append(mapping[level]);
        }
        auto prefix_end = dest.end();

        //Append the prefix to every line
        std::string_view payload{msg.payload.data(), msg.payload.size()};
        size_t index{0}, found{0};
        while(true) {
            found = payload.find(spdlog::details::os::default_eol, index);
            auto line = payload.substr(index, (found == -1 ? payload.length() : found) - index);

#ifdef HAVE_CXX_TERMINAL
            auto colored = this->_colored ? terminal::parseCharacterCodes(std::string{line}) : terminal::stripCharacterCodes(std::string{line});
#else
            auto colored = line;
#endif
            dest.append(colored.data(), colored.data() + colored.size());

            index = found;

            append(spdlog::details::os::default_eol);
            if(++index) {
                dest.append(prefix_begin, prefix_end);
            } else {
                break;
            }
        }
    }

    [[nodiscard]] std::unique_ptr<formatter> LogFormatter::clone() const {
        return std::make_unique<LogFormatter>(this->_colored);
    }
}