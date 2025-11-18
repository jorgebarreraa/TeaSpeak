#pragma once

#include <map>

namespace http {
    bool parse_url_parameters(const std::string_view& /* url */, std::map<std::string, std::string>& /* result */);
}