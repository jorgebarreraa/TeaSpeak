//
// Created by wolverindev on 15.06.18.
//

#include <fstream>
#include <sys/stat.h>
#include <misc/net.h>
#include <log/LogUtils.h>
#include "IpListManager.h"

using namespace std;
using namespace ts;

IpListManager::IpListManager(std::string file, const std::deque<std::string>& def) : file(std::move(file)), default_entries(def) { }

bool file_exists(const std::string& name) {
    struct stat buffer{};
    return (stat (name.c_str(), &buffer) == 0);
}

inline string strip(std::string message) {
    while(!message.empty()) {
        if(message[0] == ' ') {
            message = message.substr(1);
        } else if(message[message.length() - 1] == ' ') {
            message = message.substr(0, message.length() - 1);
        } else {
            break;
        }
    }
    return message;
}

bool IpListManager::reload(std::string& error) {
    if(!file_exists(this->file)) {
        ofstream os{this->file};
        if(!os) {
            error = "Could not create default file!";
            return false;
        }

        for(const auto& entry : this->default_entries) {
            os << entry << endl;
        }

        os.flush();
        os.close();
    }

    ifstream stream{this->file};
    if(!stream) {
        error = "Failed to read file!";
        return false;
    }

    string line;
    int line_number = 0;
    while(getline(stream, line)) {
        line_number++;
        line = strip(line);
        if(line.empty() || line[0] == '#') {
            continue;
        }

        IPEntry result{};
        if(!this->parse_entry(result, line)) {
            logError(0, "Failed to parse ip entry at line {} of file {}. Line: '{}'", line_number, this->file, line);
        } else {
            this->entries.push_back(result);
        }
    }

    return true;
}

bool IpListManager::contains(const sockaddr_storage &address) {
    for(const auto& entry : this->entries) {
        if(net::address_equal_ranged(address, entry.address, entry.range)) {
            return true;
        }
    }
    return false;
}

bool IpListManager::parse_entry(ts::IpListManager::IPEntry &result, const std::string &line) {
    auto seperator = line.find('/');
    auto address = line.substr(0, seperator);
    auto mask = seperator == string::npos ? "255" : line.substr(seperator + 1);

    if(!net::resolve_address(address, result.address))
        return false;

    try {
        result.range = static_cast<uint8_t>(stoll(mask));
    } catch(std::exception& ex) {
        return false;
    }
    return true;
}