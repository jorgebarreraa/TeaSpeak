#pragma once

#include <spdlog/logger.h>
#include <spdlog/sinks/base_sink.h>

namespace logger {
    class ColorCodeFormatter : public spdlog::formatter {
        public:
            void format(const spdlog::details::log_msg &msg, spdlog::memory_buf_t &dest) override {
                dest.append(msg.payload.begin(), msg.payload.end());
            }

            [[nodiscard]] std::unique_ptr<formatter> clone() const override {
                return std::unique_ptr<ColorCodeFormatter>();
            }
    };

    //TODO: Mutex really needed here?
    class TerminalSink : public spdlog::sinks::base_sink<std::mutex> {
        public:
            void sink_it_(const spdlog::details::log_msg &msg) override;
            void flush_() override;
    };

    class LogFormatter : public spdlog::formatter {
        public:
            explicit LogFormatter(bool colored) : _colored{colored} {}

            void format(const spdlog::details::log_msg &msg, spdlog::memory_buf_t &dest) override;
            [[nodiscard]] std::unique_ptr<formatter> clone() const override;

            inline bool colored() const { return this->_colored; }
            inline void colored(bool flag) { this->_colored = flag; }
        private:
            bool _colored{true};
    };
}
