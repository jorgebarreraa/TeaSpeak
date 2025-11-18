#define DEFINE_VARIABLES
#include "config.h"
#include "json.hpp"
#include "logger.h"

#include <fstream>

using namespace nlohmann;
using namespace std;

#define err(message) \
do {\
    error = string() + message;\
    return false;\
} while(0)

#define get(variable, json_object, key) \
try { \
    variable = json_object[key]; \
} catch(const json::type_error& ex) { \
    err("failed to get key " + key + ". error: " + ex.what()); \
}

bool config::load(std::string &error, const std::string &file) {
    fstream stream(file, fstream::in);
    if(!stream.good()) err("failed to open file");

    json value;
    try {
        value = json::parse(stream);
    } catch(json::exception& ex) {
        err("failed to parse file: " + ex.what());
    }

    int version;
    get(version, value, "version");
    if(version != 1)
        err("invalid version. expected 1");

    get(config::backup, value, "backup");
    if(config::backup)
        get(config::backup_directory, value, "backup-directory");
    get(config::callback_file, value, "callback_file");
    get(config::callback_argument_fail, value, "callback_argument_fail");
    get(config::callback_argument_success, value, "callback_argument_success");

    {
        json locks;
        get(locks, value, "locks");
        for(json& lock_entry : locks) {
            auto entry = make_shared<LockFile>();
            get(entry->error_id, lock_entry, "error-id");
            get(entry->timeout, lock_entry, "timeout");
            get(entry->filename, lock_entry, "filename");

            config::locking_files.push_back(entry);
        }
    }

    {
        json moves;
        get(moves, value, "moves");
        for(json& move_entry : moves) {
            auto entry = make_shared<MovingFile>();
            get(entry->error_id, move_entry, "error-id");
            get(entry->source, move_entry, "source");
            get(entry->target, move_entry, "target");

            config::moving_actions.push_back(entry);
        }
    }

    if(value.contains("permission-test-directory")) {
        get(config::permission_test_directory, value, "permission-test-directory");
    }

    logger::debug("Loaded %d locking actions and %d moving actions", config::locking_files.size(), config::moving_actions.size());
    return true;
}