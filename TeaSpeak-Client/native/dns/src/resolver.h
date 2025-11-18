#pragma once

#include <thread>
#include <mutex>
#include <functional>
#include <condition_variable>
#include <utility>

#include <event.h>

#include "./types.h"
#ifdef WIN32
    #include <WinDNS.h>
#else
    struct ub_ctx;
#endif

namespace tc::dns {
	namespace response {
		class DNSHeader;
		class DNSQuery;
		class DNSResourceRecords;
	}

	struct DNSResponseData {
#ifdef WIN32
        bool wide_string{false};
		DNS_QUERY_RESULT data{};
#else
		uint8_t* buffer{nullptr};
		size_t length{0};

		std::string parse_dns_dn(std::string& /* error */, size_t& /* index */, bool /* compression allowed */);
#endif

		~DNSResponseData();
	};

	class Resolver;
	class DNSResponse {
			friend class Resolver;
		public:
			typedef std::vector<std::shared_ptr<response::DNSResourceRecords>> rr_list_t;
			typedef std::vector<std::shared_ptr<response::DNSQuery>> q_list_t;

			DNSResponse(const DNSResponse&) = delete;
			DNSResponse(DNSResponse&&) = delete;

			bool parse(std::string& /* error */);

#ifndef WIN32
			[[nodiscard]] inline const std::string why_bogus() const { return this->bogus; }

			[[nodiscard]] inline const uint8_t* packet_data() const { return this->data->buffer; }
			[[nodiscard]] inline size_t packet_length() const { return this->data->length; }

			[[nodiscard]] inline bool is_secure() const { return this->secure_state > 0; }
			[[nodiscard]] inline bool is_secure_dnssec() const { return this->secure_state == 2; }

			[[nodiscard]] response::DNSHeader header() const;
#endif
			[[nodiscard]] q_list_t queries() const { return this->parsed_queries; }
			[[nodiscard]] rr_list_t answers() const { return this->parsed_answers; }
			[[nodiscard]] rr_list_t authorities() const { return this->parsed_authorities; }
			[[nodiscard]] rr_list_t additionals() const { return this->parsed_additionals; }
		private:
#ifndef WIN32
			DNSResponse(uint8_t /* secure state */, const char* /* bogus */, void* /* packet */, size_t /* length */);

			std::shared_ptr<response::DNSResourceRecords> parse_rr(std::string& /* error */, size_t& index, bool /* compression allowed dn */);

			std::string bogus;
			uint8_t secure_state{0};
#else
            explicit DNSResponse(std::shared_ptr<DNSResponseData>);
#endif
            std::shared_ptr<DNSResponseData> data{nullptr};

			bool is_parsed{false};
			std::string parse_error{};

			q_list_t parsed_queries;
			rr_list_t parsed_answers;
			rr_list_t parsed_authorities;
			rr_list_t parsed_additionals;
	};

	class Resolver {
		public:
			struct ResultState {
				enum value : uint8_t {
					SUCCESS = 0,

					INITIALISATION_FAILED = 0x01,

					DNS_TIMEOUT = 0x10,
					DNS_FAIL = 0x11, /* error detail is a DNS error code */
					DNS_API_FAIL = 0x12,

					TSDNS_CONNECTION_FAIL = 0x20,
					TSDNS_EMPTY_RESPONSE = 0x21,

					ABORT = 0xFF /* request has been aborted */
				};
			};

			typedef std::function<void(ResultState::value /* error */, int /* error detail */, std::unique_ptr<DNSResponse> /* response */)> dns_callback_t;
			typedef std::function<void(ResultState::value /* error */, int /* error detail */, const std::string& /* response */)> tsdns_callback_t;

			Resolver();
			virtual ~Resolver();

			bool initialize(std::string& /* error */, bool /* use hosts */, bool /* use resolv */);
			void finalize();

			void resolve_dns(const char* /* name */, const rrtype::value& /* rrtype */, const rrclass::value& /* rrclass */, const std::chrono::microseconds& /* timeout */, const dns_callback_t& /* callback */);
			void resolve_tsdns(const char* /* name */, const sockaddr_storage& /* server */, const std::chrono::microseconds& /* timeout */, const tsdns_callback_t& /* callback */);
		private:
#ifdef WIN32
	        struct dns_request;
	        struct dns_old_request_data {
                /* request might me nullptr if its beeing timeouted */
                struct dns_request* request{nullptr}; /* protected by lock */
                std::mutex* lock{nullptr};
	        };

            struct {
                HMODULE libhandle{nullptr};

                DNS_STATUS (*DnsQueryEx)(
                        _In_        PDNS_QUERY_REQUEST  pQueryRequest,
                        _Inout_     PDNS_QUERY_RESULT   pQueryResults,
                        _Inout_opt_ PDNS_QUERY_CANCEL   pCancelHandle
                ) = nullptr;

                DNS_STATUS (*DnsCancelQuery)(
                        _In_        PDNS_QUERY_CANCEL    pCancelHandle
                ) = nullptr;
            } dnsapi;
#endif

			struct dns_request {
				Resolver* resolver{nullptr};

				std::string host;
				dns::rrtype::value rrtype{dns::rrtype::Unassigned};
				dns::rrclass::value rrclass{dns::rrclass::IN};

				dns_callback_t callback{};
#ifdef WIN32
                std::wstring whost;

                /* windows 8 or newer */
                DNS_QUERY_REQUEST dns_query;
                DNS_QUERY_RESULT dns_result;
                DNS_QUERY_CANCEL dns_cancel;

                /* for old stuff */
                //std::thread resolve_thread;
                std::mutex* threaded_lock{nullptr};
                struct dns_old_request_data* thread_data{nullptr}; /* protected by threaded_lock */
#else

                int ub_id{0};
				struct ::event* register_event{nullptr};
#endif
                struct ::event* timeout_event{nullptr};
                struct ::event* processed_event{nullptr};
			};

#ifndef WIN32
			struct ub_ctx* ub_ctx = nullptr;
#endif
			struct tsdns_request {
				Resolver* resolver{nullptr};

				int socket{0};

				struct ::event* timeout_event{nullptr};
				struct ::event* event_read{nullptr};
				struct ::event* event_write{nullptr};

				std::mutex buffer_lock{};
				std::string write_buffer{};
				std::string read_buffer{};

				tsdns_callback_t callback{};
			};

			struct {
				bool loop_active{false};
				std::condition_variable condition{};
				std::mutex lock{};
				std::thread loop{};
				event_base* base = nullptr;
			} event;


			std::vector<tsdns_request*> tsdns_requests{};
			std::vector<dns_request*> dns_requests{};
			std::recursive_mutex request_lock{}; /* this is recursive because due to the instance callback resolve_dns could be called recursively */

            bool initialize_platform(std::string& /* error */, bool /* use hosts */, bool /* use resolv */);
            void finalize_platform();

			void destroy_dns_request(dns_request*);
			void destroy_tsdns_request(tsdns_request*);

			void event_loop_runner();

			void evtimer_dns_callback(dns_request* /* request */);
#ifndef WIN32
			void ub_callback(dns_request* /* request */, int /* rcode */, void* /* packet */, int /* packet_len */, int /* sec */, char* /* why_bogus */);
#else
            void dns_callback(dns_request* /* request */);
#endif

			void evtimer_tsdns_callback(tsdns_request* /* request */);
			void event_tsdns_write(tsdns_request* /* request */);
			void event_tsdns_read(tsdns_request* /* request */);

	};
}