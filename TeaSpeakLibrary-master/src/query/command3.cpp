//
// Created by wolverindev on 25.01.20.
//

#include <iostream>
#include "command3.h"

using namespace ts;

command_bulk command_parser::empty_bulk{std::string::npos, 0, ""};

bool command_parser::parse(bool command) {
    this->data = this->_command;
    this->index = std::string::npos;

    size_t index{0}, findex;
    if(command) {
        index = this->_command.find(' ', index);
        if(index == std::string::npos) {
            this->command_type = this->_command;
            return true;
        } else {
            this->command_type = this->data.substr(0, index);
        }

        index++;
    }

    this->_bulks.reserve(4);
    while(index < this->_command.size()) {
        findex = this->_command.find('|', index);
        if(findex == std::string::npos)
            findex = this->_command.size();

        this->_bulks.emplace_back(this->_bulks.size() - 1, index, this->data.substr(index, findex - index));
        index = findex + 1;
    }
    return true;
}

std::optional<size_t> command_parser::next_bulk_containing(const std::string_view &key, size_t start) const {
    if(start >= this->bulk_count()) return std::nullopt;

    auto index = this->bulk(start).command_character_index();
    auto next = this->data.find(key, index);
    if(next == std::string::npos) return std::nullopt;

    size_t upper_bulk{start + 1};
    for(; upper_bulk < this->bulk_count(); upper_bulk++)
        if(this->bulk(upper_bulk).command_character_index() > next)
            break;
    return upper_bulk - 1;
}