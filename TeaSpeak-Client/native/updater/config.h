
#pragma once

#include <deque>
#include <string>
#include <memory>
#include <optional>

namespace config {
    extern bool load(std::string& /* error */, const std::string& /* file */);

    struct LockFile {
        std::string filename;
        uint32_t timeout;
        std::string error_id;
    };

    struct MovingFile {
         std::string source; /* if source is empty it means file delete */
         std::string target;

         std::string error_id;
    };

#ifdef DEFINE_VARIABLES
    #define _extern
#else
    #define _extern extern
#endif

    _extern bool backup;
    _extern std::string backup_directory;
    _extern std::string callback_file;
    _extern std::string callback_argument_fail;
    _extern std::string callback_argument_success;

    _extern std::optional<std::string> permission_test_directory;

    _extern std::deque<std::shared_ptr<LockFile>> locking_files;
    _extern std::deque<std::shared_ptr<MovingFile>> moving_actions;
}