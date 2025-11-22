#pragma once

#include <netinet/in.h>
#include "./ip.h"

namespace lookup {
    namespace ipv6_impl {
        struct address_t {
            union {
                uint64_t address_u64[ 2];
            };

            uint16_t port;
        };

        struct converter {
            constexpr inline void operator()(address_t& result, const sockaddr_in6& addr) {
                auto addr_ptr = (uint64_t*) &addr.sin6_addr;

                result.address_u64[0] = addr_ptr[0];
                result.address_u64[1] = addr_ptr[1];

                result.port = addr.sin6_port;
            }
        };

        struct comparator {
            constexpr inline bool operator()(const address_t& a, const address_t& b) {
                return a.address_u64[0] == b.address_u64[0] && a.address_u64[1] == b.address_u64[1] && a.port == b.port;
            }
        };

        struct hash {
            constexpr inline uint8_t operator()(const sockaddr_in6& address) {
                auto addr_ptr = (uint8_t*) &address.sin6_addr;

                return (uint8_t) (addr_ptr[8] ^ addr_ptr[9]) ^ (uint8_t) (addr_ptr[15] ^ address.sin6_port);
            }
        };
    }

    template <typename T>
    using ip_v6 = ip_vx<
            T,
            sockaddr_in6,
            ipv6_impl::address_t,
            ipv6_impl::converter,
            ipv6_impl::comparator,
            ipv6_impl::hash
    >;
}
