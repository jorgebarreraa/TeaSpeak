#pragma once

#include <netinet/in.h>
#include "./ip.h"

namespace lookup {
    namespace ipv4_impl {
        union uaddress_t {
            struct {
                uint32_t address{0};
                uint16_t port{0};
            };

            uint64_t value;
        };

        struct converter {
            constexpr inline void operator()(uaddress_t& result, const sockaddr_in& addr) {
                result.address = addr.sin_addr.s_addr;
                result.port = addr.sin_port;
            }
        };

        struct comparator {
            constexpr inline bool operator()(const uaddress_t& a, const uaddress_t& b) {
                return a.value == b.value;
            }
        };

        struct hash {
            constexpr inline uint8_t operator()(const sockaddr_in& address) {
                return (address.sin_addr.s_addr & 0xFFU) ^ (address.sin_port);
            }
        };
    }

    template <typename T>
    using ip_v4 = ip_vx<
            T,
            sockaddr_in,
            ipv4_impl::uaddress_t,
            ipv4_impl::converter,
            ipv4_impl::comparator,
            ipv4_impl::hash
    >;
}
