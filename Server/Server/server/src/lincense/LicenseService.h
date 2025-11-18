#pragma once

#include <variant>
#include <thread>
#include <mutex>
#include <memory>

namespace license::client {
    class LicenseServerClient;
}

namespace google::protobuf {
    class Message;
}

namespace ts::server::license {
    struct InstanceLicenseInfo {
        bool is_old_license{false};
        std::shared_ptr<::license::License> license{nullptr};
        std::string web_certificate_revision{};

        struct metrics_ {
            size_t servers_online{0};
            size_t client_online{0};
            size_t web_clients_online{0};
            size_t bots_online{0};
            size_t queries_online{0};

            size_t speech_total{0};
            size_t speech_varianz{0};
            size_t speech_online{0};
            size_t speech_dead{0};
        } metrics;

        struct info_ {
            std::chrono::milliseconds timestamp{};
            std::string version{};
            std::string uname{};
            std::string unique_id{};
        } info;
    };


    class LicenseService {
        public:
            LicenseService();
            ~LicenseService();

            [[nodiscard]] bool initialize(std::string& /* error */);
            void shutdown();

            /* whatever it failed/succeeded */
            bool execute_request_sync(const std::chrono::milliseconds& /* timeout */);

            [[nodiscard]] inline bool verbose() const { return this->verbose_; }
            void execute_tick(); /* should not be essential to the core functionality! */
        private:
            std::chrono::steady_clock::time_point startup_timepoint_;

            enum struct request_state {
                empty,

                /* initializing */
                dns_lookup,
                connecting,

                /* connected states */
                license_validate,
                license_upgrade,
                property_update,

                /* disconnecting */
                finishing
            };
            bool verbose_{false};

            std::recursive_timed_mutex request_lock{};
            request_state request_state_{request_state::empty};
            std::shared_ptr<::license::client::LicenseServerClient> current_client{nullptr};
            std::shared_ptr<InstanceLicenseInfo> license_request_data{nullptr};

            std::condition_variable sync_request_cv;
            std::mutex sync_request_lock;

            struct _timings {
                std::chrono::system_clock::time_point last_request{};
                std::chrono::system_clock::time_point next_request{};

                std::chrono::system_clock::time_point last_succeeded{};
                size_t failed_count{0};
            } timings;

            struct _dns {
                std::shared_ptr<std::recursive_mutex> lock{nullptr};

                struct _lookup {
                    std::shared_ptr<std::recursive_mutex> lock{nullptr};
                    std::thread thread{};

                    LicenseService* handle{nullptr}; /* may be null, locked via lock */
                }* current_lookup{nullptr};
            } dns;

            std::optional<std::string> license_invalid_reason{}; /* set if the last license is invalid */

            void schedule_next_request(bool /* last request succeeded */);

            void begin_request();
            void client_send_message(::license::protocol::PacketType /* type */, ::google::protobuf::Message& /* message */);
            void handle_check_fail(const std::string& /* error */); /* might be called form the DNS loop */
            void handle_check_succeeded();

            /* if not disconnect message has been set it will just close the connection */
            void abort_request(std::lock_guard<std::recursive_timed_mutex>& /* request lock */, const std::string& /* disconnect message */);

            void abort_dns_request();
            void execute_dns_request();

            /* will be called while dns lock has been locked! */
            void handle_dns_lookup_result(bool /* success */, const std::variant<std::string, sockaddr_in>& /* data */);

            /* all callbacks bellow are called from the current_client. It will not be null while being within the callback. */
            void handle_client_connected();
            void handle_client_disconnected(const std::string& /* error */);

            void handle_message(::license::protocol::PacketType /* type */, const void* /* buffer */, size_t /* length */);
            void handle_message_license_info(const void* /* buffer */, size_t /* length */);
            void handle_message_license_update(const void* /* buffer */, size_t /* length */);
            void handle_message_property_adjustment(const void* /* buffer */, size_t /* length */);

            void send_license_validate_request();
            bool send_license_update_request();
            void send_property_update_request();
    };
}