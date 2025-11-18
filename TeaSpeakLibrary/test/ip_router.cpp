#include <assert.h>
#include <iostream>
#include <src/misc/net.h>
#include <netinet/in.h>
#include "src/misc/ip_router.h"

using namespace ts::network;

inline sockaddr_storage address(const std::string& address) {
    sockaddr_storage result{};
    memset(&result, 0, sizeof(result));
    net::resolve_address(address, result);
    return result;
}

inline void resolve(ip_router& router, const std::string& address_str) {
    std::cout << address_str << " -> " << router.resolve(address(address_str)) << std::endl;
}

/*
IPv6 & 15000 iterations
0% misses:
Time per resolve: 608ns
50% misses:
Time per resolve: 448ns
100% misses:
Time per resolve: 30ns

IPv6 & 15000 iterations & shared mutex
0% misses:
Time per resolve: 848ns
50% misses:
Time per resolve: 602ns
100% misses:
Time per resolve: 39ns

IPv6 & 15000 iterations & spin lock
0% misses:
Time per resolve: 743ns
50% misses:
Time per resolve: 510ns
100% misses:
Time per resolve: 29ns


New system:
0% misses:
Time per resolve: 149ns
Used memory: 67.155kb
50% misses:
Time per resolve: 71ns
Used memory: 32.501kb
100% misses:
Time per resolve: 14ns
Used memory: 4kb
 */

inline void benchmark_resolve() {
    const auto iterations = 500;

    std::vector<sockaddr_storage> addresses{};

    /* generate addresses */
    {
        const auto rnd = []{
            return (uint32_t) ((uint8_t) ::rand());
        };

        for(int i = 0; i < iterations; i++) {
            auto& address = addresses.emplace_back();
            address.ss_family = AF_INET;

            auto address4 = (sockaddr_in*) &address;
            address4->sin_addr.s_addr = (rnd() << 24U) | (rnd() << 16U) | (rnd() << 8U) | (rnd() << 0U);
        }
#if 0
        for(int i = 0; i < iterations; i++) {
            auto& address = addresses.emplace_back();
            address.ss_family = AF_INET6;

            auto address6 = (sockaddr_in6*) &address;
            for(auto& part : address6->sin6_addr.__in6_u.__u6_addr8)
                part = rand();
        }
#endif

    }

    for(auto missPercentage : {0, 50, 100}) {
        std::cout << missPercentage << "% misses:\n";

        ip_router rounter{};
        for(auto& address : addresses) {
            if((::rand() % 100) < missPercentage) continue;

            rounter.register_route(address, &address, nullptr);
        }

        /* resolve */
        {
            auto begin = std::chrono::system_clock::now();
            size_t index{0};
            for(auto& address : addresses) {
                index++;
                auto result = rounter.resolve(address);
                (void) result;
                //if(missPercentage == 0 && memcmp(result, &address, sizeof(address)) != 0)
                //    __asm__("nop");
            }
            auto end = std::chrono::system_clock::now();
            std::cout << "Total time: " << std::chrono::ceil<std::chrono::microseconds>(end - begin).count() << "us\n";
            std::cout << "Time per resolve: " << std::chrono::ceil<std::chrono::nanoseconds>(end - begin).count() / iterations << "ns\n";
            std::cout << "Used memory: " << rounter.used_memory() / 1024 << "kb" << std::endl;
            __asm__("nop");

            if(missPercentage == 0)
                ;//std::cout << "Tree:\n" << rounter.print_as_string() << "\n";
        }
    }



    /* resolve */
    {
        std::cout << "List iterate:\n";
        auto begin = std::chrono::system_clock::now();
        for(auto& address : addresses) {
            for(auto& addressB : addresses)
                if(memcmp(&address, &addressB, sizeof(sockaddr_storage)) == 0)
                    break;
        }
        auto end = std::chrono::system_clock::now();
        std::cout << "Total time: " << std::chrono::ceil<std::chrono::microseconds>(end - begin).count() << "us\n";
        std::cout << "Time per resolve: " << std::chrono::ceil<std::chrono::nanoseconds>(end - begin).count() / iterations << "ns\n";
    }
}

int main() {
#if 0
    ip_router rounter{};
    assert(rounter.validate_tree());

    resolve(rounter, "127.0.0.1");

    rounter.register_route(address("127.0.0.1"), (void*) 0x127001);
    assert(rounter.validate_tree());
    resolve(rounter, "127.0.0.1");

    rounter.register_route(address("127.0.0.2"), (void*) 0x127002);
    assert(rounter.validate_tree());
    resolve(rounter, "127.0.0.2");
    resolve(rounter, "127.0.1.2");

    rounter.register_route(address("127.0.1.2"), (void*) 0x127012);
    resolve(rounter, "127.0.0.2");
    resolve(rounter, "127.0.1.2");

    std::cout << "Tree:\n" << rounter.print_as_string() << "\n";

    rounter.reset_route(address("127.0.0.1"));
    assert(rounter.validate_tree());
    resolve(rounter, "127.0.0.1");
    resolve(rounter, "127.0.0.2");

    rounter.reset_route(address("127.0.0.2"));
    assert(rounter.validate_tree());
    resolve(rounter, "127.0.0.1");
    resolve(rounter, "127.0.0.2");
#endif

    benchmark_resolve();
    return 0;
}