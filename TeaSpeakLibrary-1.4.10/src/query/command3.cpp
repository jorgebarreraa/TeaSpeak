//
// Created by wolverindev on 25.01.20.
//

#include <iostream>
#include "command3.h"

using namespace ts;

bool command_parser::parse(bool command) {
    this->bulk_index = std::string::npos;

    size_t index{0}, findex;
    if(command) {
        index = this->data.find(' ', index);
        if(index == std::string::npos) {
            this->command_type = this->data;
            return true;
        } else {
            this->command_type = this->data.substr(0, index);
        }

        index++;
    }

    while(index < this->data.size()) {
        findex = this->data.find('|', index);
        if(findex == std::string::npos) {
            findex = this->data.size();
        }

        this->_bulks.emplace_back(this->_bulks.size() - 1, index, this->data.substr(index, findex - index));
        index = findex + 1;
    }
    return true;
}

std::string_view command_parser::payload_view(size_t bulk_index) const noexcept {
    if(bulk_index >= this->bulk_count()) {
        return {};
    }

    auto bulk = this->bulk(bulk_index);
    return this->data.substr(bulk.command_character_index());
}

std::optional<size_t> command_parser::next_bulk_containing(const std::string_view &key, size_t start) const {
    if(start >= this->bulk_count()) return std::nullopt;

    auto index = this->bulk(start).command_character_index();
    auto next = this->data.find(key, index);
    if(next == std::string::npos) return std::nullopt;

    size_t upper_bulk{start + 1};
    for(; upper_bulk < this->bulk_count(); upper_bulk++) {
        if(this->bulk(upper_bulk).command_character_index() > next) {
            break;
        }
    }
    return upper_bulk - 1;
}