#pragma once

#include <string>
#include <string_view>
#include <cstdint>

#ifdef WIN32
    #include <Windows.h>
    #define log_format_t _In_z_ _Printf_format_string_ const char*
#else
    #define log_format_t const char*
#endif

namespace log_helper {
    struct const_string {
            constexpr const_string(const char* value) noexcept : value(value) { }

            std::string string() const { return std::string(this->value); }
            std::string operator+(const char* other) const {
                return this->string() + other;
            }

            operator const char*() const { return this->value; }
            operator std::string() const { return this->string(); }

            const char* value;
    };

    inline std::string operator+(const std::string& left, const const_string& right) {
        return left + right.string();
    }
    inline std::string& operator+=(std::string& _this, const const_string& other) {
        return _this += other.string();
    }

    template <typename T>
    struct argument_t {
            static constexpr bool supported = false;
            static constexpr const_string value = "[unknown]";
    };

#define define_argument(type, pattern) \
    template <> \
    struct argument_t<type> { \
            static constexpr bool supported = true; \
            static constexpr const_string value = pattern; \
    };

    define_argument(char, "%s");
    define_argument(char16_t, "%S");
    define_argument(wchar_t, "%S");

    template <typename T>
    using argument = argument_t<T>;
}

namespace logger {
    namespace level {
        enum value {
            trace,
            debug,
            info,
            warn,
            error,
            fatal
        };
    }

    extern bool pipe_file(const std::string_view& /* target */);
    extern void log_raw(level::value /* level */, const char* /* format */, ...);
    extern void flush();

    template <typename... args_t>
    inline void log(level::value level, const std::string_view& format, args_t&&... arguments) {
        log_raw(level, format.data(), std::forward<args_t>(arguments)...);
    }


#define define_alias(log_level, name) \
    template <typename... args_t> \
    inline void name(const std::string_view& format, args_t... arguments) { \
        log(level::log_level, format, std::forward<args_t>(arguments)...); \
    }

    define_alias(trace, trace);
    define_alias(debug, debug);
    define_alias(info, info);
    define_alias(warn, warn);
    define_alias(error, error);
    define_alias(fatal, fatal);

#undef define_alias
}