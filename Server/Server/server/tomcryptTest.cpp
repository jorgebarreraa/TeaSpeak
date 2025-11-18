#include <tommath.h>
#include <iostream>
#include <string_view>

//#define STANDALONE

#ifndef STANDALONE
    #include <cassert>
#endif

//c++ -I. --std=c++1z test.cpp ./libtommath.a -o test
#ifdef STANDALONE
int main() {
#else
void testTomMath(){
#endif
    {
        mp_int x{}, n{}, exp{}, r{};
        mp_init_multi(&x, &n, &exp, &r, nullptr);
        mp_2expt(&exp, 1000);

        mp_read_radix(&x, "2280880776330203449294339386427307168808659578661428574166839717243346815923951250209099128371839254311904649344289668000305972691071196233379180504231889", 10);
        mp_read_radix(&n, "436860662135489324843442078840868871476482593772359054106809367217662215065650065606351911592188139644751920724885335056877706082800496073391354240530016", 10);

        auto err = mp_exptmod(&x, &exp, &n, &r);
#ifdef STANDALONE
        std::cout << "Series A: " << err << ", expected != 0\n";
#else
        //assert(err != MP_OKAY); //if this method succeed than tommath failed. Unknown why but it is so
#endif
        (void) err;
        mp_clear_multi(&x, &n, &exp, &r, nullptr);
    }
    {
        mp_int x{}, n{}, exp{}, r{};
        mp_init_multi(&x, &n, &exp, &r, nullptr);
        mp_2expt(&exp, 1000);

#if 0
        const static std::string_view n_{"\x01\x7a\xc5\x8d\x28\x7a\x61\x58\xf6\xe3\x98\x60\x2f\x81\x9c\x8a\x48\xc9\x20\xd1\x59\xe0\x24\x75\x91\x27\x9f\x52\x1e\x2c\x24\x85\xa9\xdc\x74\xfa\x0b\x36\xf9\x6c\x77\xa3\x7c\xf9\xbb\xf7\x04\xad\xa3\x84\x0d\x97\x25\x54\x19\x72\x4f\x8f\xfc\x66\xbe\x41\xda\x95"};
        const static std::string_view x_{"\xd1\xef\xf0\x16\x34\x48\x56\x53\x15\x97\xa0\x28\xbd\x13\xce\xbf\xc2\xd6\x79\x9d\x21\x81\x83\x37\x8c\xe8\xee\xee\xa1\x22\xa4\xf5\x63\x33\x53\x0c\x38\x2f\x0a\x00\x53\x20\xc7\x93\x52\xa9\xd0\xc2\xfb\xbc\xc5\xc4\xc3\x54\xad\xcb\x49\x52\xc0\xd8\x97\x32\x94\xee"};
        mp_read_unsigned_bin(&x, (unsigned char*) x_.data(), x_.length());
        mp_read_unsigned_bin(&n, (unsigned char*) n_.data(), n_.length());
#else
        const static std::string_view n_{"017ac58d287a6158f6e398602f819c8a48c920d159e0247591279f521e2c2485a9dc74fa0b36f96c77a37cf9bbf704ada3840d97255419724f8ffc66be41da95"};
        const static std::string_view x_{"d1eff016344856531597a028bd13cebfc2d6799d218183378ce8eeeea122a4f56333530c382f0a005320c79352a9d0c2fbbcc5c4c354adcb4952c0d8973294ee"};
        mp_read_radix(&x, x_.data(), 16);
        mp_read_radix(&n, n_.data(), 16);
#endif

        auto err = mp_exptmod(&x, &exp, &n, &r);
#ifdef STANDALONE
        std::cout << "Series B: " << err << ", expected != 0\n";
#else
        //assert(err != MP_OKAY); //if this method succeed than tommath failed. Unknown why but it is so
#endif
        (void) err;

        mp_clear_multi(&x, &n, &exp, &r, nullptr);
    }
}