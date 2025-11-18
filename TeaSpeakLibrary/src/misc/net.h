#pragma once

#include <string>
#include <cstring>
#include <deque>
#include <vector>
#include <tuple>
#include <stdexcept>

#ifdef WIN32
    #define _WINSOCK_DEPRECATED_NO_WARNINGS /* gethostbyname is deprecated for windows */
    #include <WS2tcpip.h>
    #include <WinSock2.h>
    #include <Windows.h>
    #include <in6addr.h>
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <netdb.h>
#endif

namespace net {
    [[nodiscard]] inline std::string to_string(const in6_addr& address) {
        char buffer[INET6_ADDRSTRLEN];
        if(!inet_ntop(AF_INET6, (void*) &address, buffer, INET6_ADDRSTRLEN)) return "";
        return std::string(buffer);
    }

    [[nodiscard]] inline std::string to_string(const in_addr& address) {
        char buffer[INET_ADDRSTRLEN];
        if(!inet_ntop(AF_INET, (void*) &address, buffer, INET_ADDRSTRLEN)) return "";
        return std::string(buffer);
    }

    [[nodiscard]] inline std::string to_string(const sockaddr_storage& address, bool port = true) {
        switch(address.ss_family) {
            case AF_INET:
                return to_string(((sockaddr_in*) &address)->sin_addr) + (port ? ":" + std::to_string(htons(((sockaddr_in*) &address)->sin_port)) : "");
            case AF_INET6:
                return to_string(((sockaddr_in6*) &address)->sin6_addr) + (port ? ":" + std::to_string(htons(((sockaddr_in6*) &address)->sin6_port)) : "");
            default:
                return "unknown_type";
        }
    }

    [[nodiscard]] inline uint16_t port(const sockaddr_storage& address) {
        switch(address.ss_family) {
            case AF_INET:
                return htons(((sockaddr_in*) &address)->sin_port);
            case AF_INET6:
                return htons(((sockaddr_in6*) &address)->sin6_port);
            default:
                return 0;
        }
    }

    [[nodiscard]] inline socklen_t address_size(const sockaddr_storage& address) {
        switch (address.ss_family) {
            case AF_INET: return sizeof(sockaddr_in);
            case AF_INET6: return sizeof(sockaddr_in6);
            default: return 0;
        }
    }

    [[nodiscard]] inline bool address_equal(const sockaddr_storage& a, const sockaddr_storage& b) {
        if(a.ss_family != b.ss_family) return false;
        if(a.ss_family == AF_INET) return ((sockaddr_in*) &a)->sin_addr.s_addr == ((sockaddr_in*) &b)->sin_addr.s_addr;
        else if(a.ss_family == AF_INET6) {
#ifdef WIN32
            return memcmp(((sockaddr_in6*) &a)->sin6_addr.u.Byte, ((sockaddr_in6*) &b)->sin6_addr.u.Byte, 16) == 0;
#else
            return memcmp(((sockaddr_in6*) &a)->sin6_addr.__in6_u.__u6_addr8, ((sockaddr_in6*) &b)->sin6_addr.__in6_u.__u6_addr8, 16) == 0;
#endif
        }
        return false;
    }

    [[nodiscard]] inline bool address_equal_ranged(const sockaddr_storage& a, const sockaddr_storage& b, uint8_t range) {
        if(a.ss_family != b.ss_family) return false;
        if(a.ss_family == AF_INET) {
            auto address_a = ((sockaddr_in*) &a)->sin_addr.s_addr;
            auto address_b = ((sockaddr_in*) &b)->sin_addr.s_addr;

            if(range > 32)
                range = 32;

            range = (uint8_t) (32 - range);

            address_a <<= range;
            address_b <<= range;

            return address_a == address_b;
        } else if(a.ss_family == AF_INET6) {
#ifdef WIN32
            throw std::runtime_error("not implemented");
            //FIXME: Implement me!
#elif defined(__x86_64__) && false
            static_assert(sizeof(__int128) == 16);
            auto address_a = (__int128) ((sockaddr_in6*) &a)->sin6_addr.__in6_u.__u6_addr32;
            auto address_b = (__int128) ((sockaddr_in6*) &b)->sin6_addr.__in6_u.__u6_addr32;

            if(range > 128)
                range = 128;
            range = (uint8_t) (128 - range);

            address_a <<= range;
            address_b <<= range;

            return address_a == address_b;
#else
            static_assert(sizeof(uint64_t) == 8);

            if(range > 128)
                range = 128;
            range = (uint8_t) (128 - range);


            auto address_ah = (uint64_t) (((sockaddr_in6*) &a)->sin6_addr.__in6_u.__u6_addr8 + 0);
            auto address_al = (uint64_t) (((sockaddr_in6*) &a)->sin6_addr.__in6_u.__u6_addr8 + 8);
            auto address_bh = (uint64_t) (((sockaddr_in6*) &b)->sin6_addr.__in6_u.__u6_addr8 + 0);
            auto address_bl = (uint64_t) (((sockaddr_in6*) &b)->sin6_addr.__in6_u.__u6_addr8 + 8);

            if(range > 64) {
                /* only lower counts */
                return (address_al << (range - 64)) == (address_bl << (range - 64));
            } else {
                return address_al == address_bl &&(address_bh << (range - 64)) == (address_ah << (range - 64));
            }
#endif
        }
        return false;
    }


    [[nodiscard]] inline bool is_ipv6(const std::string& str) {
        sockaddr_in6 sa{};
        return inet_pton(AF_INET6, str.c_str(), &(sa.sin6_addr)) != 0;
    }

    [[nodiscard]] inline bool is_ipv4(const std::string& str) {
        sockaddr_in sa{};
        return inet_pton(AF_INET, str.c_str(), &(sa.sin_addr)) != 0;
    }

    [[nodiscard]] inline bool is_anybind(sockaddr_storage& storage) {
        if(storage.ss_family == AF_INET) {
            auto data = (sockaddr_in*) &storage;
            return data->sin_addr.s_addr == 0;
        } else if(storage.ss_family == AF_INET6) {
            auto data = (sockaddr_in6*) &storage;
#ifdef WIN32
            auto& blocks = data->sin6_addr.u.Word;
            return
                    blocks[0] == 0 &&
                    blocks[1] == 0 &&
                    blocks[2] == 0 &&
                    blocks[3] == 0 &&
                    blocks[4] == 0 &&
                    blocks[5] == 0 &&
                    blocks[6] == 0 &&
                    blocks[7] == 0;
#else
            auto& blocks = data->sin6_addr.__in6_u.__u6_addr32;
            return blocks[0] == 0 && blocks[1] == 0 && blocks[2] == 0 && blocks[3] == 0;
#endif
        }
        return false;
    }

    [[nodiscard]] inline bool resolve_address(const std::string& address, sockaddr_storage& result) {
        if(is_ipv4(address)) {
            sockaddr_in s{};
            s.sin_port = 0;
            s.sin_family = AF_INET;

            auto record = gethostbyname(address.c_str());
            if(!record)
                return false;
            s.sin_addr.s_addr = ((in_addr*) record->h_addr)->s_addr;

            memcpy(&result, &s, sizeof(s));
            return true;
        } else if(is_ipv6(address)) {
            sockaddr_in6 s{};
            s.sin6_family = AF_INET6;
            s.sin6_port = 0;
            s.sin6_flowinfo = 0;
            s.sin6_scope_id = 0;

#ifdef WIN32
            auto record = gethostbyname(address.c_str());
#else
            auto record = gethostbyname2(address.c_str(), AF_INET6);
#endif
            if(!record) return false;
            s.sin6_addr = *(in6_addr*) record->h_addr;

            memcpy(&result, &s, sizeof(s));
            return true;
        } else if(address == "[::]" || address == "::") {
            sockaddr_in6 s{};
            s.sin6_family = AF_INET6;
            s.sin6_port = 0;
            s.sin6_flowinfo = 0;
            s.sin6_scope_id = 0;

            memcpy(&s.sin6_addr, &in6addr_any, sizeof(in6_addr));
            memcpy(&result, &s, sizeof(s));
            return true;
        }

        return false;
    }

    [[nodiscard]] std::vector<std::tuple<std::string, sockaddr_storage, std::string>> resolve_bindings(const std::string& bindings, uint16_t port);

    enum struct binding_result {
        ADDRESS_FREE,
        ADDRESS_USED,
        INTERNAL_ERROR
    };

    enum struct binding_type {
        TCP,
        UDP
    };

    [[nodiscard]] binding_result address_available(const sockaddr_storage& address, binding_type type);
}