#pragma once

#include <string>
#include <cassert>

namespace hex {
    inline std::string hex(const std::string& input, char beg, char end){
        assert(end - beg  == 16);

        int len = input.length() * 2;
        char output[len];
        int idx = 0;
        for (char elm : input) {
            output[idx++] = static_cast<char>(beg + ((elm >> 4) & 0x0F));
            output[idx++] = static_cast<char>(beg + ((elm & 0x0F) >> 0));
        }

        return std::string(output, len);
    }
    inline std::string hex(const std::string& input){
        size_t len = input.length() * 2;
        char output[len];
        size_t idx = 0;
        for (char elm : input) {
            auto lower = ((uint8_t) elm >> 4U) & 0xFU;
            auto upper = ((uint8_t) elm & 0xFU) >> 0U;
            output[idx++] = static_cast<char>(lower > 9 ? 'a' + (lower - 9) : '0' + lower);
            output[idx++] = static_cast<char>(upper > 9 ? 'a' + (upper - 9) : '0' + upper);
        }

        return std::string(output, len);
    }
}