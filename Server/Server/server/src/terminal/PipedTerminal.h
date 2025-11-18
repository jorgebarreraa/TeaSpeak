#pragma once

#include <string>

namespace terminal {
    extern void initialize_pipe(const std::string& /* path */);
    extern void finalize_pipe();
}