#pragma once

#include <protocol/buffers.h>
#include <netinet/in.h>
#include <functional>
#include <mutex>

#include "license.h"

namespace license::client {
    class LicenseServerClient : public std::enable_shared_from_this<LicenseServerClient> {
        public:
            enum ConnectionState {
                CONNECTING,
                INITIALIZING,
                CONNECTED,
                DISCONNECTING,

                UNCONNECTED
            };
            typedef std::function<void()> callback_connected_t;
            typedef std::function<void(protocol::PacketType /* type */, const void* /* payload */, size_t /* length */)> callback_message_t;
            typedef std::function<void(bool /* expected */, const std::string& /* reason */)> callback_disconnect_t;

            explicit LicenseServerClient(const sockaddr_in&, int /* protocol version */);
            virtual ~LicenseServerClient();

            bool start_connection(std::string& /* error */);
            void send_message(protocol::PacketType /* type */, const void* /* buffer */, size_t /* length */);
            void close_connection();

            void disconnect(const std::string& /* reason */, std::chrono::system_clock::time_point /* timeout */);
            bool await_disconnect();


            /*
             * Events will be called within the event loop.
             * All methods are save to call.
             * When close_connection or await_disconnect has been called these methods will not be called anymore.
             */
            callback_message_t callback_message{nullptr};
            callback_connected_t callback_connected{nullptr};
            callback_disconnect_t callback_disconnected{nullptr};

            const int protocol_version;
        private:
            std::mutex connection_lock{};
            ConnectionState connection_state{ConnectionState::UNCONNECTED};
            std::chrono::system_clock::time_point disconnect_timeout{};

            struct Buffer {
                static Buffer* allocate(size_t /* capacity */);
                static void free(Buffer* /* ptr */);

                void* data;
                size_t capacity;
                size_t fill;
                size_t offset;

                TAILQ_ENTRY(Buffer) tail;
            };

            /* modify everything here only within the event base, or when exited when connection_lock is locked */
            struct {
                sockaddr_in address{};
                int file_descriptor{0};

                std::thread event_dispatch{};
                struct event_base* event_base{nullptr}; /* will be cleaned up by the event loop! */
                struct event* event_read{nullptr};
                struct event* event_write{nullptr};
            } network;

            struct {
                std::mutex lock{};
                std::condition_variable notify_empty{};

                Buffer* read{nullptr}; /* must noch be accessed via lock because only the event loop uses it */
                TAILQ_HEAD(, Buffer) write;
            } buffers;

            struct {
                bool initialized{false};
                std::string crypt_key{};
            } communication;

            void callback_read(short /* events */);
            void callback_write(short /* events */);
            void callback_socket_connected();

            void cleanup_network_resources();

            void handle_data(void*, size_t);
            void handle_raw_packet(protocol::PacketType /* type */, void* /* payload */, size_t /* length */);
            void handle_handshake_packet(void* /* payload */, size_t /* length */);
    };
}