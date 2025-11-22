#pragma once

#include <string>

namespace ts::query {
    extern std::string escape(std::string);
    extern std::string unescape(std::string, bool /* throw error */);
}