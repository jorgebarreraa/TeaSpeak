#pragma once

template <typename Rep, typename Per>
inline std::string duration_to_string(std::chrono::duration<Rep, Per> ms) {
    std::string result{};

    {
        auto hours = std::chrono::duration_cast<std::chrono::hours>(ms);
        if(hours.count())
            result += std::to_string(hours.count()) + " hours ";
        ms -= hours;
    }
    {
        auto minutes = std::chrono::duration_cast<std::chrono::minutes>(ms);
        if(minutes.count())
            result += std::to_string(minutes.count()) + " minutes ";
        ms -= minutes;
    }
    {
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(ms);
        if(seconds.count())
            result += std::to_string(seconds.count()) + " seconds ";
        ms -= seconds;
    }
    if(result.empty()) {
        auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(ms);
        if(milliseconds.count())
            result = std::to_string(milliseconds.count()) + " milliseconds ";
    }
    if(result.empty()) {
        auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(ms);
        result = std::to_string(microseconds.count()) + " microseconds ";
    }
    return result.substr(0, result.length() - 1);
}
