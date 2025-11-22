#pragma once

#include <cstddef>
#include <string_view>

namespace utf8 {
    [[nodiscard]] inline ssize_t count_characters(const std::string_view& message) {
        ssize_t index{0};
        ssize_t count{0};
        while(index < message.length()){
            count++;

            auto current = (uint8_t) message[index];
            if(current >= 128) { //UTF8 check
                if(current >= 192 && (current <= 193 || current >= 245)) {
                    return -1;
                } else if(current >= 194 && current <= 223) {
                    if(message.length() - index <= 1) {
                        return -1;
                    } else if((uint8_t) message[index + 1] >= 128 && (uint8_t) message[index + 1] <= 191) {
                        index += 1; //Valid
                    } else {
                        return -1;
                    }
                } else if(current >= 224 && current <= 239) {
                    if(message.length() - index <= 2) {
                        return -1;
                    } else if((uint8_t) message[index + 1] >= 128 && (uint8_t) message[index + 1] <= 191 &&
                            (uint8_t) message[index + 2] >= 128 && (uint8_t) message[index + 2] <= 191) {
                        index += 2; //Valid
                    } else {
                        return -1;
                    }
                } else if(current >= 240 && current <= 244) {
                    if(message.length() - index <= 3) {
                        return -1;
                    } else if((uint8_t) message[index + 1] >= 128 && (uint8_t) message[index + 1] <= 191 &&
                            (uint8_t) message[index + 2] >= 128 && (uint8_t) message[index + 2] <= 191 &&
                            (uint8_t) message[index + 3] >= 128 && (uint8_t) message[index + 3] <= 191) {
                        index += 3; //Valid
                    } else {
                        return -1;
                    }
                } else {
                    return -1;
                }
            }
            index++;
        }

        return count;
    }
}