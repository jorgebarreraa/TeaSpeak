#include "converter.h"
#include <sstream>
#include <algorithm>

using namespace std;
using namespace ts;

#define CONVERTER_ST(type, m_decode, m_encode)                          \
CONVERTER_METHOD_DECODE(type, impl::converter_ ##type ##_decode) {      \
    m_decode;                                                           \
}                                                                       \
                                                                        \
CONVERTER_METHOD_ENCODE(type, impl::converter_ ##type ##_encode) {      \
    m_encode                                                            \
}

#define CONVERTER_PRIMITIVE_ST(type, m_decode) CONVERTER_ST(type,       \
    return m_decode;,                                                   \
    return std::to_string(std::any_cast<type>(value));                  \
)

CONVERTER_PRIMITIVE_ST(int8_t, std::stol(std::string{str}) & 0xFF);
CONVERTER_PRIMITIVE_ST(uint8_t, std::stoul(std::string{str}) & 0xFF);

CONVERTER_PRIMITIVE_ST(int16_t, std::stol(std::string{str}) & 0xFFFF);
CONVERTER_PRIMITIVE_ST(uint16_t, std::stoul(std::string{str}) & 0xFFFF);

CONVERTER_PRIMITIVE_ST(int32_t, std::stol(std::string{str}) & 0xFFFFFFFF);
CONVERTER_PRIMITIVE_ST(uint32_t, std::stoul(std::string{str}) & 0xFFFFFFFF);

CONVERTER_PRIMITIVE_ST(int64_t, std::stoll(std::string{str}));
CONVERTER_PRIMITIVE_ST(uint64_t, std::stoull(std::string{str}))

CONVERTER_PRIMITIVE_ST(bool, str == "1" || str == "true");
CONVERTER_PRIMITIVE_ST(float, std::stof(std::string{str}));
CONVERTER_PRIMITIVE_ST(double, std::stod(std::string{str}));
CONVERTER_PRIMITIVE_ST(long_double, std::stold(std::string{str}));

CONVERTER_ST(std__string, return std::string{str};, return std::any_cast<std__string>(value););
CONVERTER_ST(std__string_view, return str;, return std::string{std::any_cast<std__string_view>(value)};);
CONVERTER_ST(const_char__ , return str.data();, return std::string{std::any_cast<const_char__>(value)};);