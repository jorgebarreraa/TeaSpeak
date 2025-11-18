#pragma once

#include <deque>
#include <memory>

namespace bbcode::sloppy {
        extern bool has_tag(std::string message, std::deque<std::string> tag);

        inline bool has_url(const std::string& message) { return has_tag(message, {"url"}); }
        inline bool has_image(const std::string& message) { return has_tag(message, {"img"}); }
    }