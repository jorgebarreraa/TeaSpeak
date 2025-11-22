#pragma once

#include <any>
#include <string>
#include <cstddef>

namespace ts {
    typedef long double long_double;

    /* Converter stuff */
    template <typename T>
    struct converter {
        static constexpr bool supported = false;

        static constexpr std::string(*to_string)(const std::any&) = nullptr;
        static constexpr T(*from_string_view)(const std::string_view&) = nullptr;
    };

    #define DECLARE_CONVERTER(type, decode, encode)                                                             \
    template <>                                                                                                 \
    struct converter<type> {                                                                                    \
        static constexpr bool supported = true;                                                                 \
                                                                                                                \
        static constexpr std::string(*to_string)(const std::any&) = encode;                                     \
        static constexpr type(*from_string_view)(const std::string_view&) = decode;                                  \
    };

    #define CONVERTER_METHOD_DECODE(type, name) type name(const std::string_view& str)
    #define CONVERTER_METHOD_ENCODE(type, name) std::string name(const std::any& value)

    /* helper for primitive types */
    #define CONVERTER_PRIMITIVE(type)                                                                           \
    namespace impl {                                                                                            \
        CONVERTER_METHOD_DECODE(type, converter_ ##type ##_decode);                                             \
        CONVERTER_METHOD_ENCODE(type, converter_ ##type ##_encode);                                             \
    }                                                                                                           \
    DECLARE_CONVERTER(type, ::ts::impl::converter_ ##type ##_decode, ::ts::impl::converter_ ##type ##_encode);

    CONVERTER_PRIMITIVE(bool);
    CONVERTER_PRIMITIVE(float);
    CONVERTER_PRIMITIVE(double);
    CONVERTER_PRIMITIVE(long_double);

    CONVERTER_PRIMITIVE(int8_t);
    CONVERTER_PRIMITIVE(uint8_t);

    CONVERTER_PRIMITIVE(int16_t);
    CONVERTER_PRIMITIVE(uint16_t);

    CONVERTER_PRIMITIVE(int32_t);
    CONVERTER_PRIMITIVE(uint32_t);

    CONVERTER_PRIMITIVE(int64_t);
    CONVERTER_PRIMITIVE(uint64_t);

    typedef std::string std__string;
    typedef std::string_view std__string_view;
    typedef const char* const_char__;
    CONVERTER_PRIMITIVE(std__string);
    CONVERTER_PRIMITIVE(std__string_view);
    CONVERTER_PRIMITIVE(const_char__);

    /* const expr char literal */
    template <int length>
    struct converter<char[length]> {
        using type = char[length];
        static constexpr bool supported = true;

        static constexpr std::string(*to_string)(const std::any&) = [](const std::any& value) { return std::string(std::any_cast<const char*>(value), length - 1); };
    };

    #undef CONVERTER_PRIMITIVE
}

#define DEFINE_CONVERTER_ENUM(class, size_type)                                                                 \
namespace ts {                                                                                                  \
    template <>                                                                                                 \
    struct converter<class> {                                                                                   \
        static constexpr bool supported = true;                                                                 \
                                                                                                                \
        static constexpr std::string(*to_string)(const std::any&) = [](const std::any& val) {                   \
            return std::to_string((size_type) std::any_cast<class>(val));                                       \
        };                                                                                                      \
                                                                                                                \
        static constexpr class(*from_string_view)(const std::string_view&) = [](const std::string_view& val) {  \
            return ((class(*)(const std::string_view&)) ts::converter<size_type>::from_string_view)(val);       \
        };                                                                                                      \
    };                                                                                                          \
}
/* DO NOT REMOVE ME (NL warning) */