#pragma once

#include <any>
#include <string>
#include <cstddef>
#include <chrono>

#define DEFINE_CONVERTER_ENUM(class, size_type)                                                                 \
namespace ts {                                                                                                  \
    template <>                                                                                                 \
    struct converter<class> {                                                                                   \
        static constexpr bool supported{true};                                                                  \
        static constexpr bool references{false};                                                                \
                                                                                                                \
        static constexpr std::string(*to_string)(const std::any&) = [](const std::any& val) {                   \
            return std::to_string(std::any_cast<class>(val));                                                   \
        };                                                                                                      \
        static constexpr class(*from_string_view)(const std::string_view&) = [](const std::string_view& val) {  \
            return ((class(*)(const std::string_view&)) ts::converter<size_type>::from_string_view)(val);       \
        };                                                                                                      \
    };                                                                                                          \
}

namespace ts {
    typedef long double long_double;
    typedef long long unsigned int long_long_unsigned_int_t;
    /* Converter stuff */
    template <typename T>
    struct converter {
        static constexpr bool supported{false};
        static constexpr bool references{false};

        static constexpr std::string(*to_string)(const std::any&) = nullptr;
        static constexpr T(*from_string_view)(const std::string_view&) = nullptr;
    };

#define DECLARE_CONVERTER(type, decode, encode, references_)                                                    \
    template <>                                                                                                 \
    struct converter<type> {                                                                                    \
        static constexpr bool supported{true};                                                                  \
        static constexpr bool references{references_};                                                          \
                                                                                                                \
        static constexpr std::string(*to_string)(const std::any&) = encode;                                     \
        static constexpr type(*from_string_view)(const std::string_view&) = decode;                             \
    };

#define CONVERTER_METHOD_DECODE(type, name) type name(const std::string_view& str)
#define CONVERTER_METHOD_ENCODE(type, name) std::string name(const std::any& value)

    /* helper for primitive types */
#define CONVERTER_PRIMITIVE(type, references)                                                                   \
    namespace impl {                                                                                            \
        CONVERTER_METHOD_DECODE(type, converter_ ##type ##_decode);                                             \
        CONVERTER_METHOD_ENCODE(type, converter_ ##type ##_encode);                                             \
    }                                                                                                           \
    DECLARE_CONVERTER(type, ::ts::impl::converter_ ##type ##_decode, ::ts::impl::converter_ ##type ##_encode, references);

    CONVERTER_PRIMITIVE(bool, false);
    CONVERTER_PRIMITIVE(float, false);
    CONVERTER_PRIMITIVE(double, false);
    CONVERTER_PRIMITIVE(long_double, false);

    CONVERTER_PRIMITIVE(int8_t, false);
    CONVERTER_PRIMITIVE(uint8_t, false);

    CONVERTER_PRIMITIVE(int16_t, false);
    CONVERTER_PRIMITIVE(uint16_t, false);

    CONVERTER_PRIMITIVE(int32_t, false);
    CONVERTER_PRIMITIVE(uint32_t, false);

    CONVERTER_PRIMITIVE(int64_t, false);
    CONVERTER_PRIMITIVE(uint64_t, false);
#if __x86_64__
    CONVERTER_PRIMITIVE(long_long_unsigned_int_t, false);
#endif
    typedef std::string std__string;
    typedef std::string_view std__string_view;
    typedef const char* const_char__;
    CONVERTER_PRIMITIVE(std__string, false);
    CONVERTER_PRIMITIVE(std__string_view, true);
    CONVERTER_PRIMITIVE(const_char__, true);

    /* const expr char literal */
    template <int length>
    struct converter<char[length]> {
        using type = char[length];
        static constexpr bool supported{true};

        static constexpr std::string(*to_string)(const std::any&) = [](const std::any& value) { return std::string(std::any_cast<const char*>(value), length - 1); };
    };

    /* We're not enabling this since we don't transport the unit
    template <typename Rep, typename Period>
    struct converter<std::chrono::duration<Rep, Period>> {
        using type = std::chrono::duration<Rep, Period>;
        static constexpr bool supported{true};

        static constexpr std::string(*to_string)(const std::any&) = [](const std::any& value) { return std::to_string(std::any_cast<type>(value)); };
    }
    */

#undef CONVERTER_PRIMITIVE
}
/* DO NOT REMOVE ME (NL warning) */