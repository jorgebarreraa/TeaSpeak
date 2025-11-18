#pragma once

#include <cstddef>
#include <string>
#include <functional>
#include <variant>
#include "./src/resolver.h"

namespace tc::dns {
	struct ServerAddress {
		std::string host;
		uint16_t port;
	};
	typedef std::function<void(bool /* success */, const std::variant<std::string, ServerAddress>& /* data */)> cr_callback_t;

	inline std::string next_subdomain_level(const std::string& input) {
        auto next_level = input.find('.');
        if(next_level == std::string::npos) {
            /* invalid input */
            return input;
        }

        if(input.find('.', next_level + 1) == std::string::npos) {
            /* we're already on the top level */
            return input;
        }

        return input.substr(next_level + 1);
	}

	extern void cr_ip(Resolver& resolver, const ServerAddress& address, const cr_callback_t& callback);
	extern void cr_srv(Resolver& resolver, const ServerAddress& address, const cr_callback_t& callback, const std::string& application = "_ts._udp");
	extern void cr_tsdns(Resolver& resolver, const ServerAddress& address, const cr_callback_t& callback);
	extern void cr(Resolver& resolver, const tc::dns::ServerAddress& address, const tc::dns::cr_callback_t& callback);
}