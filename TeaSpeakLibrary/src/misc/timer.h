#pragma once


#ifndef TIMING_DISABLED
    #define TIMING_REPORT(expression) \
        expression

    #define TIMING_START(_name) \
    struct { \
        struct entry { \
            std::string name; \
            std::chrono::system_clock::time_point ts; \
        }; \
         \
        std::string name; \
        std::chrono::system_clock::time_point begin; \
        std::chrono::system_clock::time_point end; \
        std::deque<entry> timings; \
    } _name ##_timings; \
    _name ##_timings.begin = std::chrono::system_clock::now(); \

    #define TIMING_STEP(name, step) \
    name ##_timings.timings.push_back({step, std::chrono::system_clock::now()});

    #define TIMING_FINISH_U(_name, unit, unit_name) \
    ([&](){ \
        _name ##_timings.end = std::chrono::system_clock::now(); \
        std::string result; \
        result = "timings for " + _name ##_timings.name + ": "; \
        result += std::to_string(std::chrono::duration_cast<std::chrono::unit>(_name ##_timings.end - _name ##_timings.begin).count()) + unit_name; \
         \
        auto tp = _name ##_timings.begin; \
        for(const auto& entry : _name ##_timings.timings) { \
            result += "\n  "; \
            result += "- " + entry.name + ": "; \
            result += "@" + std::to_string(std::chrono::duration_cast<std::chrono::unit>(entry.ts - _name ##_timings.begin).count()) + unit_name; \
            result += ": " + std::to_string(std::chrono::duration_cast<std::chrono::unit>(entry.ts - tp).count()) + unit_name; \
            tp = entry.ts; \
        } \
        return result; \
    })()

    #define TIMING_FINISH(_name) TIMING_FINISH_U(_name, milliseconds, "ms")
#else
    #define TIMING_REPORT(expression)
    #define TIMING_START(_name)
    #define TIMING_STEP(name, step)
    #define TIMING_FINISH_U(_name, unit, unit_name)
    #define TIMING_FINISH(_name)
#endif
/* FIX the "backslash-newline at end of file" warning */
