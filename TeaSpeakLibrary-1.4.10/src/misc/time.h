#pragma once

#include <string>
#include <chrono>
#include <deque>

namespace period {
    std::chrono::nanoseconds parse(const std::string&, std::string&);
}