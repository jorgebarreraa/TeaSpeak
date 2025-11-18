#pragma once

#include <string>
#include <string_view>
#include <cstring>
#include <cassert>

#ifdef NO_OPEN_SSL
    #define SHA_DIGEST_LENGTH (20)
    #define SHA256_DIGEST_LENGTH (32)
    #define SHA512_DIGEST_LENGTH (64)

    #define DECLARE_DIGEST(name, _unused_, digestLength)                                        \
    namespace tomcrypt {                                                                        \
        extern void name(const char* input, size_t length, uint8_t* result);    \
    }                                                                                           \
    inline std::string name(const std::string& input) {                                         \
        uint8_t result[digestLength];                                                           \
        tomcrypt::name(input.data(), input.length(), result);                                   \
        auto _result = std::string((const char*) result, (size_t) digestLength);                \
        assert(_result.length() == digestLength);                                               \
        return _result;                                                                         \
    }                                                                                           \
                                                                                                \
    inline std::string name(const char* input, int64_t length = -1) {                           \
        if(length == -1) length = strlen(input);                                                \
        uint8_t result[digestLength];                                                           \
        tomcrypt::name(input, length, result);                                                  \
        auto _result = std::string((const char*) result, (size_t) digestLength);                \
        assert(_result.length() == digestLength);                                               \
        return _result;                                                                         \
    }                                                                                           \
                                                                                                \
                                                                                                \
    inline void name(const char* input, size_t length, uint8_t* result) {       \
        tomcrypt::name(input, length, result);                                                  \
    }
#else
    #include <openssl/sha.h>

    #define DECLARE_DIGEST(name, method, digestLength)                                      \
    inline std::string name(const std::string& input) {                                     \
        u_char buffer[digestLength];                                                        \
        method((u_char*) input.data(), input.length(), buffer);                             \
        return std::string((const char*) buffer, (size_t) digestLength);                             \
    }                                                                                       \
                                                                                            \
    inline std::string name(const char* input, ssize_t length = -1) {                       \
        if(length == -1) length = strlen(input);                                            \
        return name(std::string(input, (size_t) length));                                            \
    }                                                                                       \
                                                                                        \
    inline void name(const char* input, size_t length, uint8_t* result) {   \
        method((u_char*) input, length, result);                                            \
    }
#endif

namespace digest {
    DECLARE_DIGEST(sha1, SHA1, SHA_DIGEST_LENGTH)
    DECLARE_DIGEST(sha256, SHA256, SHA256_DIGEST_LENGTH)
    DECLARE_DIGEST(sha512, SHA512, SHA512_DIGEST_LENGTH)
}

#undef DECLARE_DIGEST
