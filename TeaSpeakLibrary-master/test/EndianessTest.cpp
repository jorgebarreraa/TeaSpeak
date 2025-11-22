#include <iostream>
#include <misc/endianness.h>
#include <cassert>

union TypeBuffer {
    char buf[8];
    union {
        char n8_pad[7];
        uint8_t un8;
    };
    union {
        char n16_pad[6];
        uint16_t un16;
    };
    union {
        char n32_pad[4];
        uint32_t un32;
    };
    union {
        uint64_t un64;
    };
};

#define _T(size, n, rn)                         \
le2be ##size(0x ##n, buffer.buf);               \
assert(buffer.un ##size == 0x ##rn);            \
assert(be2le ##size(buffer.buf) == 0x ##n)

#define T8(_1) _T(8, _1, _1)
#define T16(_1, _2) _T(16, _1 ##_2, _2 ##_1)
#define T32(_1, _2, _3, _4) _T(32, _1 ##_2 ##_3 ##_4, _4 ##_3 ##_2 ##_1)
#define T64(_1, _2, _3, _4, _5, _6, _7, _8) _T(64, _1 ##_2 ##_3 ##_4 ##_5 ##_6 ##_7 ##_8, _8 ##_7 ##_6 ##_5 ##_4 ##_3 ##_2 ##_1)

int main() {
    TypeBuffer buffer{};
    static_assert(sizeof(buffer) == 8, "");

    T8(FF);
    T8(00);
    T8(AF);
    T8(7F);

    T16(FF, 00);
    T16(00, FF);
    T16(7F, 8F);
    T16(23, CA);

    T32(FF, FF, FF, FF);
    T32(FF, FF, 00, FF);
    T32(F0, FF, 00, 0F);
    T32(F0, CF, 00, 0F);

    T64(FF, FF, FF, FF, FF, FF, FF, FF);
    T64(FF, 00, 00, 00, 00, 00, 00, 00);

    T64(FF, FF, 00, FF, 00, 00, 00, 00);
    T64(F0, FF, 00, 0F, AF, BF, 0F, CF);
    T64(F0, CF, 00, 0F, A2, 00, 03, 22);
}