#pragma once

#include <memory>
#include <string>
#include <cstdint>
#include <stdexcept>
#ifdef WIN32
    #include <WinSock2.h>
    #include <Windows.h>
    #include <WinDNS.h>
#else
    #include <netinet/in.h>
#endif

#include "./types.h"

struct in6_addr;
namespace tc::dns {
	class DNSResponse;
	struct DNSResponseData;

	namespace response {
		#ifndef WIN32
		class DNSHeader {
				friend class tc::dns::DNSResponse;
			public:
				[[nodiscard]] inline size_t id() const { return this->field(0); }

				[[nodiscard]] inline bool is_answer() const { return this->field(1) & 0x1UL; }
				[[nodiscard]] inline bool is_authoritative_answer() const { return (uint8_t) ((this->field(1) >> 5UL) & 0x01UL); }
				[[nodiscard]] inline bool is_truncation() const { return (uint8_t) ((this->field(1) >> 6UL) & 0x01UL); }
				[[nodiscard]] inline bool is_recursion_desired() const { return (uint8_t) ((this->field(1) >> 7UL) & 0x01UL); }
				[[nodiscard]] inline bool is_recursion_available() const { return (uint8_t) ((this->field(1) >> 8UL) & 0x01UL); }

				[[nodiscard]] inline uint8_t query_type() const { return (uint8_t) ((this->field(1) >> 1UL) & 0x07UL); }
				[[nodiscard]] inline uint8_t response_code() const { return (uint8_t) ((this->field(1) >> 12UL) & 0x07UL); }

				[[nodiscard]] inline uint16_t query_count() const { return ntohs(this->field(2)); }
				[[nodiscard]] inline uint16_t answer_count() const { return ntohs(this->field(3)); }
				[[nodiscard]] inline uint16_t authority_count() const { return ntohs(this->field(4)); }
				[[nodiscard]] inline uint16_t additional_count() const { return htons(this->field(5)); }
			private:
				[[nodiscard]] uint16_t field(int index) const;

				explicit DNSHeader(const DNSResponse* response) : response{response} {}
				const DNSResponse* response{nullptr};
		};
		#endif

		class DNSQuery {
				friend class tc::dns::DNSResponse;
			public:
				[[nodiscard]] inline std::string qname() const { return this->name; }
				[[nodiscard]] inline rrtype::value qtype() const { return this->type; }
				[[nodiscard]] inline rrclass::value qclass() const { return this->klass; }
			private:
				DNSQuery(std::string name, rrtype::value type, rrclass::value klass) : name{std::move(name)}, type{type}, klass{klass} {}

				std::string name;
				rrtype::value type;
				rrclass::value klass;
		};

		class DNSResourceRecords {
				friend class tc::dns::DNSResponse;
			public:
				[[nodiscard]] inline std::string qname() const {
#ifdef WIN32
	                return std::string{this->nrecord->pName};
#else
					return this->name;
#endif
				}
				[[nodiscard]] inline rrtype::value atype() const {
#ifdef WIN32
	                return static_cast<rrtype::value>(this->nrecord->wType);
#else
					return this->type;
#endif
				}
				[[nodiscard]] inline rrclass::value aclass() const {
#ifdef WIN32
				    return static_cast<rrclass::value>(1);
#else
                    return this->klass;
#endif
				}
				[[nodiscard]] inline uint16_t attl() const {
#ifdef WIN32
				    return (uint16_t) this->nrecord->dwTtl;
#else
					return this->ttl;
#endif
				}

			#ifndef WIN32
				[[nodiscard]] const uint8_t* payload_data() const;
				[[nodiscard]] inline size_t payload_length() const { return this->length; }
				[[nodiscard]] inline size_t payload_offset() const { return this->offset; }
			#else
				[[nodiscard]] inline PDNS_RECORDA native_record() const { return this->nrecord; }
                [[nodiscard]] bool is_wide_string() const;
			#endif

				[[nodiscard]] inline std::shared_ptr<DNSResponseData> dns_data() const {
					return this->data;
				}

				template <typename T>
				[[nodiscard]] inline T parse() const {
					if(T::type != this->atype())
						throw std::logic_error{"parser type mismatch"};
					return T{this};
				}
			private:
				std::shared_ptr<DNSResponseData> data{nullptr};

			#ifdef WIN32
				DNSResourceRecords(std::shared_ptr<DNSResponseData>, PDNS_RECORDA);

				PDNS_RECORDA nrecord{nullptr};
			#else
				DNSResourceRecords(std::shared_ptr<DNSResponseData>, size_t, size_t, uint32_t, std::string , rrtype::value, rrclass::value);

				size_t offset{0};
				size_t length{0};

				uint32_t ttl;

				std::string name;
				rrtype::value type;
				rrclass::value klass;
			#endif
		};

		namespace rrparser {
			struct base {
				protected:
					explicit base(const DNSResourceRecords* handle) : handle{handle} {}
					const DNSResourceRecords* handle{nullptr};
			};

			struct named_base : public base {
				public:
					[[nodiscard]] bool is_valid();
					[[nodiscard]] std::string name();
				protected:
					explicit named_base(const DNSResourceRecords* handle) : base{handle} {}
			};

			#define define_parser(name, base, ...)                                              \
			struct name : public base {                                                         \
				friend class response::DNSResourceRecords;                                      \
				public:                                                                         \
					static constexpr auto type = rrtype::name;                                  \
	                __VA_ARGS__                                                                 \
				private:                                                                        \
					explicit name(const DNSResourceRecords* handle) : base{handle} {}           \
			}

			define_parser(A, base,
			              [[nodiscard]] bool is_valid();
					      [[nodiscard]] std::string address_string();

			              [[nodiscard]] in_addr address();
			);

			define_parser(AAAA, base,
			              [[nodiscard]] bool is_valid();
			              [[nodiscard]] std::string address_string();
			              [[nodiscard]] in6_addr address();
			);

			define_parser(SRV, base,
			              [[nodiscard]] bool is_valid();

			              [[nodiscard]] uint16_t priority();
			              [[nodiscard]] uint16_t weight();
			              [[nodiscard]] uint16_t target_port();
			              [[nodiscard]] std::string target_hostname();
			);

			define_parser(CNAME, named_base);
		};
	}
}
