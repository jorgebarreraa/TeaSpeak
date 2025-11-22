//
// Created by WolverinDEV on 16/04/2020.
//

#ifndef WIN32
#include <unistd.h>
#include <fcntl.h>
#endif

#include "./net.h"

namespace helpers {
    inline void strip(std::string& message) {
        while(!message.empty()) {
            if(message[0] == ' ')
                message = message.substr(1);
            else if(message[message.length() - 1] == ' ')
                message = message.substr(0, message.length() - 1);
            else break;
        }
    }

    inline std::deque<std::string> split(const std::string& message, char delimiter) {
        std::deque<std::string> result{};
        size_t found, index = 0;
        do {
            found = message.find(delimiter, index);
            result.push_back(message.substr(index, found - index));
            index = found + 1;
        } while(index != 0);
        return result;
    }
}

std::vector<std::tuple<std::string, sockaddr_storage, std::string>> net::resolve_bindings(const std::string& bindings, uint16_t port) {
    auto binding_list = helpers::split(bindings, ',');
    std::vector<std::tuple<std::string, sockaddr_storage, std::string>> result;
    result.reserve(binding_list.size());

    for(auto& address : binding_list) {
        helpers::strip(address);

        sockaddr_storage element{};
        memset(&element, 0, sizeof(element));
        if(!resolve_address(address, element)) {
            result.emplace_back(address, element, "address resolve failed");
            continue;
        }

        if(element.ss_family == AF_INET) {
            ((sockaddr_in*) &element)->sin_port = htons(port);
        } else if(element.ss_family == AF_INET6) {
            ((sockaddr_in6*) &element)->sin6_port = htons(port);
        }
        result.emplace_back(address, element, "");
    }
    return result;
}

#ifndef WIN32
net::binding_result net::address_available(const sockaddr_storage& address, binding_type type) {
    int file_descriptor{0};
    net::binding_result result{binding_result::INTERNAL_ERROR};
    int disable{0}, enable{1};

    file_descriptor = socket(address.ss_family, type == binding_type::TCP ? SOCK_STREAM : SOCK_DGRAM, 0);
    if(file_descriptor <= 0)
        goto cleanup_exit;

    fcntl(file_descriptor, F_SETFD, FD_CLOEXEC); /* just to ensure */
    setsockopt(file_descriptor, SOL_SOCKET, SO_REUSEADDR, &disable, sizeof(int));
    //setsockopt(file_descriptor, SOL_SOCKET, SO_REUSEPORT, &disable, sizeof(int));
    if(type == binding_type::UDP && address.ss_family == AF_INET6)
        setsockopt(file_descriptor, IPPROTO_IPV6, IPV6_V6ONLY, &enable, sizeof(int));

    if(::bind(file_descriptor, (const sockaddr*) &address, net::address_size(address)) != 0) {
        result = binding_result::ADDRESS_USED;
    } else {
        result = binding_result::ADDRESS_FREE;
    }

    if(type == binding_type::TCP) {
        if(::listen(file_descriptor, 1) != 0)
            result = binding_result::ADDRESS_USED;
    }

    cleanup_exit:
    if(file_descriptor > 0)
        ::close(file_descriptor);
    return result;
}
#endif