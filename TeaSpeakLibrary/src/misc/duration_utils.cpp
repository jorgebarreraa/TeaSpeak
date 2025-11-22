#include <vector>
#include <functional>
#include "time.h"

using namespace std;
using namespace chrono;

struct TimeEntry {
    std::string indice;
    std::function<nanoseconds(const std::string&)> parser;
};

auto parsers = std::vector<TimeEntry>({
                          {"h",  [](const std::string& number) -> nanoseconds { return hours(stoll(number)); }},
                          {"m",  [](const std::string& number) -> nanoseconds { return minutes(stoll(number)); }},
                          {"s",  [](const std::string& number) -> nanoseconds { return seconds(stoll(number)); }},
                          {"ms", [](const std::string& number) -> nanoseconds { return milliseconds(stoll(number)); }},
                          {"us", [](const std::string& number) -> nanoseconds { return microseconds(stoll(number)); }},
                          {"ns", [](const std::string& number) -> nanoseconds { return nanoseconds(stoll(number)); }}
});

std::chrono::nanoseconds period::parse(const std::string& input, std::string& error) {
    nanoseconds result{};

    size_t index = 0;
    do {
        auto found = input.find(':', index);
        auto str = input.substr(index, found - index);

        auto indiceIndex = str.find_first_not_of("0123456789");
        if(indiceIndex == std::string::npos) {
            error = "Missing indice for " + str + " at " + to_string(index);
            return nanoseconds(0);
        }
        auto indice = str.substr(indiceIndex);
        auto number = str.substr(0, indiceIndex);

        bool foundIndice = false;
        for(const auto& parser : parsers) {
            if(parser.indice == indice) {
                if(number.length() == 0) {
                    error = "Invalid number at " + to_string(index);
                    return nanoseconds(0);
                }
                result += parser.parser(number);
                foundIndice = true;
                break;
            }
        }
        if(!foundIndice) {
            error = "Invalid indice for " + str + " at " + to_string(index + indiceIndex);
            return nanoseconds(0);
        }

        index = found + 1;
    } while(index != 0);

    return result;
}