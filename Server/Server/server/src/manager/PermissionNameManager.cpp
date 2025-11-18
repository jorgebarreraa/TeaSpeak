#include <experimental/filesystem>
#include <fstream>
#include <log/LogUtils.h>
#include "PermissionNameMapper.h"

using namespace ts::permission;
using namespace std;
namespace fs = std::experimental::filesystem;

PermissionNameMapper::PermissionNameMapper() {}
PermissionNameMapper::~PermissionNameMapper() {}


std::string PermissionNameMapper::permission_name(const Type::value& type, const ts::permission::PermissionType &permission) const {
    const static std::string unknown = "unknown";

    if(type < 0 || type > Type::MAX)
        return unknown;

    if(permission < 0 || permission >= PermissionType::permission_id_max)
        return unknown;

    return this->mapping[type][permission].mapped_name;
}

std::string PermissionNameMapper::permission_name_grant(const Type::value& type, const ts::permission::PermissionType &permission) const {
    const static std::string unknown = "unknown";

    if(type < 0 || type > Type::MAX)
        return unknown;

    if(permission < 0 || permission >= PermissionType::permission_id_max)
        return unknown;

    return this->mapping[type][permission].grant_name;
}

bool PermissionNameMapper::initialize(const std::string &file, std::string &error) {
    auto file_path = fs::u8path(file);
    if(!fs::exists(file)) {
        error = "file does not exists";
        return false;
    }

    ifstream istream(file_path);
    if(!istream.good()) {
        error = "failed to open file";
        return false;
    }

    array<map<string, string>, PermissionType::permission_id_max> mapping;

    string line;
    auto current_type = Type::MAX;
    size_t line_index = 0;

    std::string key, value;
    while(istream) {
        line.clear();
        getline(istream, line);
        line_index++;
        if(line.empty() || line[0] == '#')
            continue;

        auto seperator = line.find(':');
        if(seperator == -1) {
            logWarning(LOG_INSTANCE, "Invalid permission mapping line {}: {}", line_index, line);
            continue;
        }

        key = line.substr(0, seperator);
        value = line.substr(seperator + 1);

        if(key == "group") {
            try {
                auto index = stol(value);
                if(index < 0 || index >= Type::MAX) {
                    logWarning(LOG_INSTANCE, "Invalid permission group at {}. Value is not in bounds: {}", line_index, line);
                    continue;
                }

                current_type = (Type::value) index;
                continue;
            } catch(std::exception& ex) {
                logWarning(LOG_INSTANCE, "Invalid permission group at {}. Value is not a number: {}", line_index, line);
                continue;
            }
        } else if(key == "mapping") {
            if(current_type == Type::MAX) {
                logWarning(LOG_INSTANCE, "Invalid permission mapping entry at line {} (No group set): {}", line_index, line);
                continue;
            }
            seperator = value.find(':');
            if(seperator == -1) {
                logWarning(LOG_INSTANCE, "Invalid permission mapping entry at line {} (Missing colon): {}", line_index, line);
                continue;
            }
            key = value.substr(0, seperator);
            value = value.substr(seperator + 1);

            mapping[current_type][key] = value;
        } else {
            logWarning(LOG_INSTANCE, "Invalid permission mapping line at {}. Key is unknown: {}", line_index, line);
            continue;
        }
    }

    /* lets build the index */
    for(Type::value type = Type::MIN; type < Type::MAX; (*(int*) &type)++) {
        auto& array = this->mapping[type];
        auto& map = mapping[type];

        for(PermissionType permission = PermissionType::permission_id_min; permission < PermissionType::permission_id_max; (*(uint16_t *)&permission)++) {
            auto data = permission::resolvePermissionData(permission);

            array[permission].mapped_name = map.count(data->name) > 0 ? map[data->name] : data->name;
            array[permission].grant_name = "i_needed_modify_power_" + array[permission].mapped_name.substr(2);
        }
    }

    return true;
}