#include "./utils.h"
#include "./src/response.h"

#include <map>
#include <vector>
#include <deque>
#include <tuple>
#include <cassert>
#include <mutex>
#include <memory>
#include <iostream>
#include <random>
#include <cstring>

#ifndef WIN32
	#include <arpa/inet.h>
#else
	#include <Ws2ipdef.h>
	#include <ip2string.h>
#endif

using namespace tc::dns;
namespace parser = tc::dns::response::rrparser;

//-------------------------- IP-Resolve
struct CrIpStatus {
        std::mutex pending_lock;
        uint8_t pending{0};

        ServerAddress original{"", 0};

        std::tuple<bool, std::string> a{false, "unset"};
        std::tuple<bool, std::string> aaaa{false, "unset"};

        cr_callback_t callback;
        std::function<void(const std::shared_ptr<CrIpStatus>&)> finish_callback;

        void one_finished(const std::shared_ptr<CrIpStatus>& _this) {
            assert(&*_this == this);

            std::lock_guard lock{this->pending_lock};
            if(--pending == 0)
                this->finish_callback(_this);
        }
};

void cr_ip_finish(const std::shared_ptr<CrIpStatus>& status) {
    if(std::get<0>(status->a)) {
        status->callback(true, ServerAddress{std::get<1>(status->a), status->original.port});
    } else if(std::get<0>(status->aaaa)) {
        status->callback(true, ServerAddress{std::get<1>(status->aaaa), status->original.port});
    } else {
        status->callback(false, "failed to resolve an IP for " + status->original.host + ": A{" + std::get<1>(status->a) + "} AAAA{" + std::get<1>(status->aaaa) + "}");
    }
}

void tc::dns::cr_ip(Resolver& resolver, const ServerAddress& address, const cr_callback_t& callback) {
    auto status = std::make_shared<CrIpStatus>();

    status->original = address;
    status->finish_callback = cr_ip_finish;
    status->callback = callback;

    /* general pending so we could finish our method */
    status->pending++;

    status->pending++;
    resolver.resolve_dns(address.host.c_str(), rrtype::A, rrclass::IN, std::chrono::seconds{5}, [status, &resolver](Resolver::ResultState::value state, int code, std::unique_ptr<DNSResponse> data){
        if(state != 0) {
            status->a = {false, "A query failed. State: " + std::to_string(state) + ", Code: " + std::to_string(code)};
            status->one_finished(status);
            return;
        }
        std::string error;
        if(!data->parse(error)) {
            status->a = {false, "A query failed. State: " + std::to_string(state) + ", Code: " + std::to_string(code)};
            status->one_finished(status);
            return;
        }

        for(const auto& answer : data->answers()) {
            if(answer->atype() != rrtype::A){
                std::cerr << "Received a non A record answer in A query!" << std::endl;
                continue;
            }

            auto data = answer->parse<parser::A>();
            if(!data.is_valid())
                continue;

            status->a = {true, data.address_string()};
            status->one_finished(status);
            return;
        }

        status->a = {false, "empty response"};
        status->one_finished(status);
    });

    status->pending++;
    resolver.resolve_dns(address.host.c_str(), rrtype::AAAA, rrclass::IN, std::chrono::seconds{5}, [status, &resolver](Resolver::ResultState::value state, int code, std::unique_ptr<DNSResponse> data){
        if(state != 0) {
            status->aaaa = {false, "AAAA query failed. State: " + std::to_string(state) + ", Code: " + std::to_string(code)};
            status->one_finished(status);
            return;
        }

        std::string error;
        if(!data->parse(error)) {
            status->aaaa = {false, "failed to parse AAAA query reponse: " + error};
            status->one_finished(status);
            return;
        }

        for(const auto& answer : data->answers()) {
            if(answer->atype() != rrtype::AAAA){
                std::cerr << "Received a non AAAA record answer in AAAA query!" << std::endl;
                continue;
            }

            auto data = answer->parse<parser::AAAA>();
            if(!data.is_valid())
                continue;

            status->aaaa = {true, data.address_string()};
            status->one_finished(status);
            return;
        }

        status->aaaa = {false, "empty response"};
        status->one_finished(status);
        return;
    });

    status->one_finished(status);
}

//-------------------------- SRV-Resolve
static std::random_device srv_rnd_dev;
static std::mt19937 srv_rnd(srv_rnd_dev());

/* connect resolve for TS3 srv records */
void tc::dns::cr_srv(Resolver& resolver, const ServerAddress& address, const cr_callback_t& callback, const std::string& application) {
    auto query = application + "." + address.host;
    resolver.resolve_dns(query.c_str(), rrtype::SRV, rrclass::IN, std::chrono::seconds{5}, [callback, address, &resolver](Resolver::ResultState::value state, int code, std::unique_ptr<DNSResponse> data){
        if(state != 0) {
            callback(false, "SRV query failed. State: " + std::to_string(state) + ", Code: " + std::to_string(code));
            return;
        }

        std::string error;
        if(!data->parse(error)) {
            callback(false, "failed to parse srv query reponse: " + error);
            return;
        }

        struct SrvEntry {
                uint16_t weight;
                std::string target;
                uint16_t port;
        };
        std::map<uint16_t, std::vector<SrvEntry>> entries{};

        auto answers = data->answers();
        for(const auto& answer : answers) {
            if(answer->atype() != rrtype::SRV) {
                std::cerr << "Received a non SRV record answer in SRV query (" << rrtype::name(answer->atype()) << ")!" << std::endl;
                continue;
            }

            auto srv = answer->parse<parser::SRV>();
            entries[srv.priority()].push_back({srv.weight(), srv.target_hostname(), srv.target_port()});
        }

        if(entries.empty()) {
            callback(false, "empty response");
            return;
        }

        std::deque<std::tuple<uint16_t, SrvEntry>> results{};
        for(auto [priority, pentries] : entries) {
            uint32_t count = 0;
            for(const auto& entry : pentries) {
	            #ifdef WIN32
                count += max((size_t) entry.weight, 1UL);
				#else
	            count += std::max((size_t) entry.weight, 1UL);
				#endif
            }

            std::uniform_int_distribution<std::mt19937::result_type> dist(0, (uint32_t) (count - 1));
            auto index = dist(srv_rnd);

            count = 0;
            for(const auto& entry : pentries) {
	            #ifdef WIN32
                count += max((size_t) entry.weight, 1UL);
	            #else
	            count += std::max((size_t) entry.weight, 1UL);
	            #endif
                if(count > index) {
                    count = -1;
                    results.emplace_back(priority, entry);
                    break;
                }
            }
        }
        assert(!results.empty());

        std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) { return std::get<0>(a) < std::get<0>(b); });

        //TODO: Resolve backup stuff as well
        auto target = std::get<1>(*results.begin());
        cr_ip(resolver, {
                target.target,
                target.port == 0 ? address.port : target.port
        }, [callback](bool success, auto response) {
            if(!success) {
                //TODO: Use the backup stuff?
                callback(false, "failed to resolve dns pointer: " + std::get<std::string>(response));
                return;
            }

            callback(true, std::get<ServerAddress>(response));
        });
    });
}

//-------------------------- TSDNS-Resolve
void tc::dns::cr_tsdns(tc::dns::Resolver &resolver, const tc::dns::ServerAddress &address, const tc::dns::cr_callback_t &callback) {
    auto root = next_subdomain_level(address.host);
    cr_srv(resolver, {root, 0}, [&resolver, callback, address](bool success, auto data){
        if(!success) {
            callback(false, "failed to resolve tsdns address: " + std::get<std::string>(data));
            return;
        }
        auto tsdns_host = std::get<ServerAddress>(data);

        sockaddr_storage tsdns_address{};
        memset(&tsdns_address, 0, sizeof(tsdns_address));

        auto tsdns_a6 = (sockaddr_in6*) &tsdns_address;
        auto tsdns_a4 = (sockaddr_in*) &tsdns_address;
#ifdef WIN32
        PCSTR terminator{nullptr};
        if(RtlIpv6StringToAddressA(tsdns_host.host.c_str(), &terminator, (in6_addr*) &tsdns_a6->sin6_addr) == 0)
#else
        if(inet_pton(AF_INET6, tsdns_host.host.c_str(), &tsdns_a6->sin6_addr) == 1)
#endif
        {
            tsdns_a6->sin6_family = AF_INET6;
            tsdns_a6->sin6_port = htons(tsdns_host.port == 0 ? 41144 : tsdns_host.port);
        }
#ifdef WIN32
        else if(RtlIpv4StringToAddressA(tsdns_host.host.c_str(), false, &terminator, (in_addr*) &tsdns_a4->sin_addr) == 0)
#else
        else if(inet_pton(AF_INET, tsdns_host.host.c_str(), &(tsdns_a4->sin_addr)) == 1)
#endif
        {
            tsdns_a4->sin_family = AF_INET;
            tsdns_a4->sin_port = htons(tsdns_host.port == 0 ? 41144 : tsdns_host.port);
        } else {
            callback(false, "invalid tsdns host: " + tsdns_host.host);
            return;
        }

        auto query = address.host;
        std::transform(query.begin(), query.end(), query.begin(), tolower);
        resolver.resolve_tsdns(query.c_str(), tsdns_address, std::chrono::seconds{5}, [callback, query, address](Resolver::ResultState::value error, int detail, const std::string& response) {
            if(error == Resolver::ResultState::SUCCESS) {
                if(response == "404") {
                    callback(false, "no record found for " + query);
                } else {
                    std::string host{response};
                    std::string port{"$PORT"};

                    //TODO: Backup IP-Addresses?
                    if(host.find(',') != -1)
                        host = std::string{host.begin(), host.begin() + host.find(',')};

                    auto colon_index = host.rfind(':');
                    if(colon_index > 0 && (host[colon_index - 1] == ']' || host.find(':') == colon_index)) {
                        port = host.substr(colon_index + 1);
                        host = host.substr(0, colon_index);
                    }

                    ServerAddress resp{host, 0};
                    if(port == "$PORT") {
                        resp.port = address.port;
                    } else {
                        try {
                            resp.port = (uint16_t) stoul(port);
                        } catch(const std::exception&) {
                            callback(false, "failed to parse response: " + response + " Failed to parse port: " + port);
                            return;
                        }
                    }
                    callback(true, resp);
                }
            } else {
                callback(false, "query failed. Code: " + std::to_string(error) + "," + std::to_string(detail) + ": " + response);
            }
        });
    }, "_tsdns._tcp");
}

//-------------------------- Full-Resolve

struct CrStatus {
        enum State {
                PENDING,
                FAILED,
                SUCCESS
        };

        std::recursive_mutex pending_lock; /* do_done could be called recursively because DNS request could answer instant! */
        uint8_t pending{0};
        bool finished{false};

        tc::dns::ServerAddress address;
        tc::dns::cr_callback_t callback;

        ~CrStatus() {
            assert(this->pending == 0);
        }

        void do_done(const std::shared_ptr<CrStatus>& _this) {
            std::lock_guard lock{pending_lock};
            this->finished |= this->try_answer(_this); //May invokes next DNS query

            assert(this->pending > 0);
            if(--this->pending == 0 && !this->finished) { //Order matters we have to decrease pensing!
                this->print_status();
                this->callback(false, "no results");
                this->finished = true;
                return;
            }
        }

        typedef std::tuple<bool, std::function<void(const std::shared_ptr<CrStatus>&)>> flagged_executor_t;

        flagged_executor_t execute_subsrv_ts;
        std::tuple<State, std::string, tc::dns::ServerAddress> subsrv_ts;

        flagged_executor_t execute_subsrv_ts3;
        std::tuple<State, std::string, tc::dns::ServerAddress> subsrv_ts3;

        flagged_executor_t execute_tsdns;
        std::tuple<State, std::string, tc::dns::ServerAddress> tsdns;

        flagged_executor_t execute_root_tsdns;
        std::tuple<State, std::string, tc::dns::ServerAddress> root_tsdns;

        flagged_executor_t execute_subdomain;
        std::tuple<State, std::string, tc::dns::ServerAddress> subdomain;

        //Only after subsrc and tsdns failed
        flagged_executor_t execute_rootsrv;
        std::tuple<State, std::string, tc::dns::ServerAddress> rootsrv;

        //Only after subsrc and tsdns failed
        flagged_executor_t execute_rootdomain;
        std::tuple<State, std::string, tc::dns::ServerAddress> rootdomain;

#define try_answer_test(element, executor) \
	if(std::get<0>(element) == State::SUCCESS) { \
		this->call_answer(std::get<2>(element)); \
		return true; \
	} else if(std::get<0>(element) == State::PENDING) { \
		if(!std::get<0>(executor)) { \
			std::get<0>(executor) = true; \
			if(!!std::get<1>(executor)) { \
				std::get<1>(executor)(_this); \
				return false; \
			} else { \
				std::get<1>(element) = "No executor"; \
				std::get<0>(element) = State::FAILED; \
			} \
		} else { \
            return false;                                   \
		} \
	}

        bool try_answer(const std::shared_ptr<CrStatus>& _this) {
            if(this->finished) {
                return true;
            }

            try_answer_test(this->subsrv_ts, this->execute_subsrv_ts);
            try_answer_test(this->subsrv_ts3, this->execute_subsrv_ts3);
            try_answer_test(this->tsdns, this->execute_tsdns);
            try_answer_test(this->root_tsdns, this->execute_root_tsdns);
            try_answer_test(this->subdomain, this->execute_subdomain);
            try_answer_test(this->rootsrv, this->execute_rootsrv);
            try_answer_test(this->rootdomain, this->execute_rootdomain);
            return false;
        }

        #define answer_log(element, executor) \
	        if(!std::get<0>(executor)) \
	            std::cout << #element << ": not executed" << std::endl; \
	        else if(std::get<0>(element) == State::PENDING) \
                std::cout << #element << ": pending" << std::endl; \
            else if(std::get<0>(element) == State::FAILED) \
                std::cout << #element << ": failed: " << std::get<1>(element) << std::endl; \
            else \
                std::cout << #element << ": success: " << std::get<2>(element).host << ":" << std::get<2>(element).port << std::endl;

        void print_status() {
            answer_log(this->subsrv_ts, this->execute_subsrv_ts);
            answer_log(this->subsrv_ts3, this->execute_subsrv_ts3);
            answer_log(this->tsdns, this->execute_tsdns);
            answer_log(this->root_tsdns, this->execute_root_tsdns);
            answer_log(this->subdomain, this->execute_subdomain);
            answer_log(this->rootsrv, this->execute_rootsrv);
            answer_log(this->rootdomain, this->execute_rootdomain);
        }

        void call_answer(const tc::dns::ServerAddress& data) {
            this->print_status();
	        this->callback(true, data);
	    }
};

void tc::dns::cr(Resolver& resolver, const tc::dns::ServerAddress& address, const tc::dns::cr_callback_t& callback) {
    auto status = std::make_shared<CrStatus>();
    status->address = address;
    status->callback = callback;
    status->pending++;

    status->execute_subsrv_ts = {
            false,
            [&resolver](const std::shared_ptr<CrStatus>& status) {
                //std::cout << "Execute subsrc ts" << std::endl;
                status->pending++;
                tc::dns::cr_srv(resolver, status->address, [status](bool success, auto data) {
                    if(success) {
                        status->subsrv_ts = {CrStatus::SUCCESS, "", std::get<ServerAddress>(data)};
                    } else {
                        status->subsrv_ts = {CrStatus::FAILED, std::get<std::string>(data), {}};
                    }
                    status->do_done(status);
                }, "_ts._udp");
            }
    };
    /* execute */
    std::get<0>(status->execute_subsrv_ts) = true;

    status->execute_subsrv_ts3 = {
            false,
            [&resolver](const std::shared_ptr<CrStatus>& status) {
                //std::cout << "Execute subsrc ts3" << std::endl;
                status->pending++;
                tc::dns::cr_srv(resolver, status->address, [status](bool success, auto data) {
                    if(success) {
                        status->subsrv_ts3 = {CrStatus::SUCCESS, "", std::get<ServerAddress>(data)};
                    } else {
                        status->subsrv_ts3 = {CrStatus::FAILED, std::get<std::string>(data), {}};
                    }
                    status->do_done(status);
                }, "_ts3._udp");
            }
    };
    /* execute */
    std::get<0>(status->execute_subsrv_ts3) = true;

    status->execute_subdomain = {
            false,
            [&resolver](const std::shared_ptr<CrStatus>& status) {
                //std::cout << "Execute subdomain" << std::endl;
                status->pending++;
                tc::dns::cr_ip(resolver, status->address, [status](bool success, auto data) {
                    if(success) {
                        status->subdomain = {CrStatus::SUCCESS, "", std::get<ServerAddress>(data)};
                    } else {
                        status->subdomain = {CrStatus::FAILED, std::get<std::string>(data), {}};
                    }
                    status->do_done(status);
                });
            }
    };
    /* execute */
    //Will be autoamticall be executed after the SRV stuff
    //std::get<0>(status->execute_subdomain) = true;

    status->execute_tsdns = {
            false,
            [&resolver](const std::shared_ptr<CrStatus>& status) {
                //std::cout << "Execute tsdns" << std::endl;
                status->pending++;

                tc::dns::cr_tsdns(resolver, status->address, [status](bool success, auto data) {
                    if(success) {
                        status->tsdns = {CrStatus::SUCCESS, "", std::get<ServerAddress>(data)};
                    } else {
                        status->tsdns = {CrStatus::FAILED, std::get<std::string>(data), {}};
                    }
                    status->do_done(status);
                });
            }
    };
    /* execute */
    //Execute the TSDNS request right at the beginning because it could hang sometimes
    std::get<0>(status->execute_tsdns) = true;

    auto root_domain = tc::dns::next_subdomain_level(status->address.host);
    if(root_domain != status->address.host) {
        status->execute_root_tsdns = {
                false,
                [&resolver, root_domain](const std::shared_ptr<CrStatus>& status) {
                    std::cout << "Execute tsdns (root)" << std::endl;
                    status->pending++;

                    tc::dns::cr_tsdns(resolver, {
                            root_domain,
                            status->address.port
                    }, [status](bool success, auto data) {
                        std::cout << "Done tsdns (root)" << std::endl;
                        if(success) {
                            status->root_tsdns = {CrStatus::SUCCESS, "", std::get<ServerAddress>(data)};
                        } else {
                            status->root_tsdns = {CrStatus::FAILED, std::get<std::string>(data), {}};
                        }
                        status->do_done(status);
                    });
                }
        };

        status->execute_rootsrv = {
                false,
                [&resolver](const std::shared_ptr<CrStatus>& status) {
                    //std::cout << "Execute root srv" << std::endl;
                    status->pending++;

                    tc::dns::cr_srv(resolver, {
                            tc::dns::next_subdomain_level(status->address.host),
                            status->address.port
                    }, [status](bool success, auto data) {
                        if(success) {
                            status->rootsrv = {CrStatus::SUCCESS, "", std::get<ServerAddress>(data)};
                        } else {
                            status->rootsrv = {CrStatus::FAILED, std::get<std::string>(data), {}};
                        }
                        status->do_done(status);
                    }, "_ts3._udp");
                }
        };

        status->execute_rootdomain = {
                false,
                [&resolver](const std::shared_ptr<CrStatus>& status) {
                    //std::cout << "Execute root domain" << std::endl;
                    status->pending++;

                    tc::dns::cr_ip(resolver,{
                            tc::dns::next_subdomain_level(status->address.host),
                            status->address.port
                    }, [status](bool success, auto data) {
                        if(success) {
                            status->rootdomain = {CrStatus::SUCCESS, "", std::get<ServerAddress>(data)};
                        } else {
                            status->rootdomain = {CrStatus::FAILED, std::get<std::string>(data), {}};
                        }
                        status->do_done(status);
                    });
                }
        };
    }

    /* Only execute after every executor has been registered! */
    std::get<1>(status->execute_subsrv_ts)(status);
    std::get<1>(status->execute_subsrv_ts3)(status);
    //std::get<1>(status->execute_subdomain)(status);
    std::get<1>(status->execute_tsdns)(status);

    status->do_done(status);
}