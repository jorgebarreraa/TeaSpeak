#pragma once

#include <netinet/in.h>
#include <deque>
#include <ThreadPool/Thread.h>
#include <ThreadPool/Mutex.h>
#include <event.h>
#include <condition_variable>
#include <misc/net.h>
#include <protocol/ringbuffer.h>
#include <misc/task_executor.h>
#include "./voice/DatagramPacket.h"
#include "Definitions.h"
#include <shared_mutex>

namespace ts {
    namespace server {
        class VirtualServer;
        class VoiceServer;
        class VoiceClient;
        class POWHandler;

        class VoiceServerSocket : public std::enable_shared_from_this<VoiceServerSocket> {
            public:
                /**
                 * Note: Only access event_read or event_write when the socket mutex is acquired or
                 *       or within the event loop!
                 */
                struct NetworkEvents {
                    VoiceServerSocket* socket;
                    struct event* event_read{nullptr};
                    struct event* event_write{nullptr};

                    NetworkEvents(VoiceServerSocket* socket) : socket{socket} {};
                    NetworkEvents(const NetworkEvents&) = delete;
                    NetworkEvents(NetworkEvents&&) = delete;
                    ~NetworkEvents();
                };

                explicit VoiceServerSocket(VoiceServer* server, sockaddr_storage address);
                virtual ~VoiceServerSocket();

                [[nodiscard]] inline auto is_active() const { return this->file_descriptor > 0; };
                [[nodiscard]] inline const sockaddr_storage& address() const { return this->address_; };

                /**
                 * Create a new UDP server socket on the target address.
                 */
                [[nodiscard]] bool activate(std::string& /* error */);

                /**
                 * Deactivate the binding if activated.
                 * Note: This will block until all active network events have been processed.
                 */
                void deactivate();

                inline void send_datagram(udp::DatagramPacket* datagram) {
                    assert(!datagram->next_packet);
                    datagram->next_packet = nullptr;

                    std::lock_guard lock{this->mutex};
                    if(!this->file_descriptor) {
                        udp::DatagramPacket::destroy(datagram);
                        return;
                    }

                    *this->write_datagram_tail = datagram;
                    this->write_datagram_tail = &datagram->next_packet;
                    this->enqueue_network_write();
                }

                inline void enqueue_client_write(std::weak_ptr<VoiceClient> client) {
                    std::lock_guard lock{this->mutex};
                    if(!this->file_descriptor) {
                        return;
                    }

                    this->write_client_queue.push_back(std::move(client));
                    this->enqueue_network_write();
                }

            private:
                ServerId server_id;
                VoiceServer* server;
                sockaddr_storage address_;

                std::mutex mutex{};
                int file_descriptor{0};
                std::vector<std::unique_ptr<NetworkEvents>> network_events{};
                size_t network_write_index{0};

                udp::DatagramPacket* write_datagram_head{nullptr};
                udp::DatagramPacket** write_datagram_tail{&this->write_datagram_head};
                std::deque<std::weak_ptr<VoiceClient>> write_client_queue;

                inline udp::DatagramPacket* pop_dg_write_queue() {
                    std::lock_guard lock{this->mutex};
                    if(!this->write_datagram_head) {
                        return nullptr;
                    }

                    auto packet = std::exchange(this->write_datagram_head, this->write_datagram_head->next_packet);
                    if(!this->write_datagram_head) {
                        assert(this->write_datagram_tail == &packet->next_packet);
                        this->write_datagram_tail = &this->write_datagram_head;
                    }

                    return packet;
                }

                inline bool pop_voice_write_queue(std::shared_ptr<VoiceClient>& result) {
                    std::lock_guard lock{this->mutex};

                    auto it_begin = this->write_client_queue.begin();
                    auto it_end = this->write_client_queue.end();
                    auto it = it_begin;

                    while(it != it_end) {
                        result = it->lock();
                        if(result) {
                            this->write_client_queue.erase(it_begin, ++it);
                            return it != it_end;
                        }
                        it++;
                    }

                    if(it_begin != it_end) {
                        this->write_client_queue.erase(it_begin, it_end);
                    }
                    return false;
                }

                /**
                 * Enqueue a write event.
                 * Attention: The socket mutex must be locked!
                 */
                inline void enqueue_network_write() {
                    assert(!this->network_events.empty());
                    auto write_event = this->network_events[this->network_write_index++ % this->network_events.size()]->event_write;
                    event_add(write_event, nullptr);
                }

                static void network_event_read(int, short, void *);
                static void network_event_write(int, short, void *);
        };

        class VoiceServer {
                friend class VoiceServerSocket;
                friend class POWHandler; /* TODO: Still needed? May use some kind of callback */
            public:
                explicit VoiceServer(const std::shared_ptr<VirtualServer>& server);
                ~VoiceServer();

                bool start(const std::deque<sockaddr_storage>&, std::string&);
                bool stop(const std::chrono::milliseconds& flushTimeout = std::chrono::milliseconds{1000});

                [[nodiscard]] std::shared_ptr<VoiceClient> findClient(ClientId);
                [[nodiscard]] std::shared_ptr<VoiceClient> findClient(sockaddr_in* addr, bool lock);
                [[nodiscard]] std::shared_ptr<VoiceClient> findClient(sockaddr_in6* addr, bool lock);
                [[nodiscard]] inline std::shared_ptr<VoiceClient> findClient(sockaddr_storage* address, bool lock = true) {
                    switch(address->ss_family) {
                        case AF_INET:
                            return this->findClient((sockaddr_in*) address, lock);

                        case AF_INET6:
                            return this->findClient((sockaddr_in6*) address, lock);

                        default:
                            return nullptr;
                    }
                }

                [[nodiscard]] inline auto getSockets() {
                    std::lock_guard lock{this->sockets_mutex};
                    return this->sockets;
                }

                [[nodiscard]] inline std::shared_ptr<VirtualServer> get_server() { return this->server; }

                void tickHandshakingClients();
                void execute_resend(const std::chrono::system_clock::time_point& /* now */, std::chrono::system_clock::time_point& /* next resend */);
                bool unregisterConnection(std::shared_ptr<VoiceClient>);
            private:
                std::unique_ptr<POWHandler> pow_handler;
                std::shared_ptr<VirtualServer> server{nullptr};

                bool running{false};

                std::shared_mutex sockets_mutex{};
                std::deque<std::shared_ptr<VoiceServerSocket>> sockets{};

                task_id handshake_tick_task{0};

                std::recursive_mutex connectionLock;
                std::deque<std::shared_ptr<VoiceClient>> activeConnections;

                void handleClientAddressChange(
                        const std::shared_ptr<VoiceClient>& /* voice client */,
                        const sockaddr_storage& /* new address */,
                        const udp::pktinfo_storage& /* remote address info */
                );
        };
    }
}