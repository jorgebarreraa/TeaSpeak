#pragma once

#include <string>
#include <iostream>

namespace base64 {
    /**
    * Encodes a given string in Base64
    * @param input The input string to Base64-encode
    * @param inputSize The size of the input to decode
    * @return A Base64-encoded version of the encoded string
    */
    extern std::string encode(const char* input, size_t inputSize);

    /**
    * Encodes a given string in Base64
    * @param input The input string to Base64-encode
    * @return A Base64-encoded version of the encoded string
    */
    inline std::string encode(const std::string& input) { return encode(input.data(), input.size()); }
    inline std::string encode(const std::string_view& input) { return encode(input.data(), input.size()); }


    /**
    * Decodes a Base64-encoded string.
    * @param input The input string to decode
    * @return A string (binary) that represents the Base64-decoded data of the input
    */
    extern std::string decode(const char* input, size_t size);

    /**
    * Decodes a Base64-encoded string.
    * @param input The input string to decode
    * @return A string (binary) that represents the Base64-decoded data of the input
    */
    inline std::string decode(const std::string& input) { return decode(input.data(), input.size()); }
    inline std::string decode(const std::string_view& input) { return decode(input.data(), input.size()); }

    //A–Z, a–z, 0–9, + und /
    inline bool validate(const std::string& input) {
        for(char c : input) {
            if(c >= 'A' && c <= 'Z') continue;
            if(c >= 'a' && c <= 'z') continue;
            if(c >= '0' && c <= '9') continue;
            if(c == '+' || c == '/' || c == '=') continue;

            return false;
        }
        return true;
    }
}
inline std::string base64_encode(const char* input, const unsigned long inputSize) { return base64::encode(input, inputSize); }
inline std::string base64_encode(const std::string& input) { return base64::encode(input.c_str(), (unsigned long) input.size()); }