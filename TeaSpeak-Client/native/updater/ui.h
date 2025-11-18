#pragma once

#include <string>

namespace ui {
    enum struct FileBlockedResult {
        UNSET,
        PROCESSES_CLOSED,
        NOT_IMPLEMENTED,
        CANCELED,
        INTERNAL_ERROR
    };

#ifdef WIN32
    extern void init_win32();
#endif
    extern FileBlockedResult open_file_blocked(const std::string&);
}