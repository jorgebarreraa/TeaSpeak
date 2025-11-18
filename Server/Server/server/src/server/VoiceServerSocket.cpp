//
// Created by WolverinDEV on 15/04/2021.
//

#include "POWHandler.h"
#include <thread>
#include <algorithm>
#include <unistd.h>
#include <fcntl.h>
#include "../client/voice/VoiceClient.h"
#include <log/LogUtils.h>
#include <misc/endianness.h>
#include "src/VirtualServerManager.h"
#include "../InstanceHandler.h"
#include "./GlobalNetworkEvents.h"

using namespace std;
using namespace std::chrono;
using namespace ts::server;
using namespace ts::buffer;
using namespace ts;

VoiceServerSocket::NetworkEvents::~NetworkEvents() {
    auto event_read_ = std::exchange(this->event_read, nullptr);
    auto event_write_ = std::exchange(this->event_write, nullptr);

    if(event_read_) {
        event_free(event_read_);
    }

    if(event_write_) {
        event_free(event_write_);
    }
}

VoiceServerSocket::VoiceServerSocket(VoiceServer *server, sockaddr_storage address) : server{server}, server_id{server->get_server()->getServerId()}, address_{address} { }

VoiceServerSocket::~VoiceServerSocket() {
    /* just to ensure and to clean up pending writes */
    this->deactivate();
}

bool VoiceServerSocket::activate(std::string &error) {
    this->file_descriptor = socket(this->address_.ss_family, SOCK_DGRAM, 0);
    if(this->file_descriptor <= 0) {
        this->file_descriptor = 0;
        error = "failed to allocate new socket";
        return false;
    }

    int enable = 1, disable = 0;
    if(setsockopt(this->file_descriptor, SOL_SOCKET, SO_REUSEADDR, &disable, sizeof(int)) < 0) {
        logError(server_id, "Could not disable flag reuse address for bind {}!", net::to_string(this->address_));
    }

    /*
    if(setsockopt(this->file_descriptor, SOL_SOCKET, SO_REUSEPORT, &disable, sizeof(int)) < 0) {
        logError(server_id, "Could not disable flag reuse port for bind {}!", net::to_string(this->address_));
    }
    */

    /* We're never sending over MTU size packets! */
    int pmtu{IP_PMTUDISC_DO};
    setsockopt(this->file_descriptor, IPPROTO_IP, IP_MTU_DISCOVER, &pmtu, sizeof(pmtu));

    if(fcntl(this->file_descriptor, F_SETFD, FD_CLOEXEC) < 0) {
        error = "failed to enable FD_CLOEXEC";
        goto bind_failed;
    }

    if(this->address_.ss_family == AF_INET6) {
        if(setsockopt(this->file_descriptor, IPPROTO_IPV6, IPV6_RECVPKTINFO, &enable, sizeof(enable)) < 0) {
            error = "failed to enable IPV6_RECVPKTINFO";
            goto bind_failed;
        }

        if(setsockopt(this->file_descriptor, IPPROTO_IPV6, IPV6_V6ONLY, &enable, sizeof(enable)) < 0) {
            error = "failed to enable IPV6_V6ONLY";
            goto bind_failed;
        }
    } else {
        if(setsockopt(this->file_descriptor, IPPROTO_IP, IP_PKTINFO, &enable, sizeof(enable)) < 0) {
            error = "failed to enable IP_PKTINFO";
            goto bind_failed;
        }
    }

    if(::bind(this->file_descriptor, (const sockaddr*) &this->address_, net::address_size(this->address_)) < 0) {
        error = "bind failed: " + std::string{strerror(errno)} + " (" + std::to_string(errno) + ")";
        goto bind_failed;
    }

    fcntl(this->file_descriptor, F_SETFL, fcntl(this->file_descriptor, F_GETFL, 0) | O_NONBLOCK);

    {
        const auto& network_loop = serverInstance->network_event_loop();
        const auto network_event_count = std::min(network_loop->loop_count(), ts::config::threads::voice::events_per_server);

        std::lock_guard write_lock{this->mutex};
        NetworkEventLoopUseList* read_use_list{nullptr};
        NetworkEventLoopUseList* write_use_list{nullptr};
        for(size_t index{0}; index < network_event_count; index++) {
            auto events = std::make_unique<NetworkEvents>(this);
            events->event_read = network_loop->allocate_event(this->file_descriptor, EV_READ | EV_PERSIST, VoiceServerSocket::network_event_read, &*events, &read_use_list);
            events->event_write = network_loop->allocate_event(this->file_descriptor, EV_WRITE, VoiceServerSocket::network_event_write, &*events, &write_use_list);

            if(!events->event_read) {
                logError(server_id, "Failed to allocate network read event for voice server binding {}", net::to_string(this->address_));
                continue;
            }

            if(!events->event_write) {
                logError(server_id, "Failed to allocate network write event for voice server binding {}", net::to_string(this->address_));
                continue;
            }

            event_add(events->event_read, nullptr);
            this->network_events.emplace_back(std::move(events));
        }

        network_loop->free_use_list(read_use_list);
        network_loop->free_use_list(write_use_list);
        if(this->network_events.empty()) {
            error = "failed to register any network events";
            goto bind_failed;
        }
    }

    return true;

    bind_failed:
    this->deactivate();
    return false;
}

void VoiceServerSocket::deactivate() {
    std::unique_lock write_lock{this->mutex};
    auto network_events_ = std::move(this->network_events);
    auto file_descriptor_ = std::exchange(this->file_descriptor, 0);

    this->write_client_queue.clear();
    while(this->write_datagram_head) {
        auto datagram = std::exchange(this->write_datagram_head, this->write_datagram_head->next_packet);
        udp::DatagramPacket::destroy(datagram);
    }
    this->write_datagram_tail = &this->write_datagram_head;
    write_lock.unlock();

    /*
     * Finish all active/pending events before we clear them.
     * Since we moved these events out of network_events the can't get rescheduled.
     */
    for(const auto& binding : network_events_) {
        if(binding->event_read) {
            event_del_block(binding->event_read);
        }

        if(binding->event_write) {
            event_del_block(binding->event_write);
        }
    }

    /* Will free all events. */
    network_events_.clear();

    /* Close the file descriptor after all network events have been finished*/
    if(file_descriptor_ > 0) {
        ::close(file_descriptor_);
    }
}


template <int MHS>
struct IOData {
    int file_descriptor = 0;
    iovec vector{};
    struct msghdr message{};
    char message_headers[MHS]{};

    IOData() {
        /* Speed is key here, we dont need to zero paddings! */
#if 0
        memset(&this->vector, 0, sizeof(this->vector));
        memset(&this->message, 0, sizeof(this->message));
        memset(this->message_headers, 0, sizeof(this->message_headers));
#endif

        this->vector.iov_base = nullptr;
        this->vector.iov_len = 0;

        this->message.msg_name = nullptr;
        this->message.msg_namelen = 0;

        this->message.msg_iov = &vector;
        this->message.msg_iovlen = 1;

        this->message.msg_control = this->message_headers;
        this->message.msg_controllen = sizeof(this->message_headers);
    }
};

template <int MHS>
inline ssize_t write_datagram(IOData<MHS>& io, const sockaddr_storage& address, const udp::pktinfo_storage* info, size_t length, const void* buffer) {
    io.message.msg_flags = 0;
    io.message.msg_name = (void*) &address;
    io.message.msg_namelen = address.ss_family == AF_INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);

    io.vector.iov_len = length;
    io.vector.iov_base = (void*) buffer;

    if(info) {
        auto cmsg = CMSG_FIRSTHDR(&io.message);
        if(address.ss_family == AF_INET) {
            cmsg->cmsg_level = IPPROTO_IP;
            cmsg->cmsg_type = IP_PKTINFO;
            cmsg->cmsg_len = CMSG_LEN(sizeof(in_pktinfo));

            memcpy(CMSG_DATA(cmsg), info, sizeof(in_pktinfo));

            io.message.msg_controllen = CMSG_SPACE(sizeof(in_pktinfo));
        } else if(address.ss_family == AF_INET6) {
            cmsg->cmsg_level = IPPROTO_IPV6;
            cmsg->cmsg_type = IPV6_PKTINFO;
            cmsg->cmsg_len = CMSG_LEN(sizeof(in6_pktinfo));

            memcpy(CMSG_DATA(cmsg), info, sizeof(in6_pktinfo));

            io.message.msg_controllen = CMSG_SPACE(sizeof(in6_pktinfo));
        } else if(address.ss_family == 0) {
            return length; /* address is unset (testing ip loss i guess) */
        }
    } else {
        io.message.msg_controllen = 0;
    }

    auto status = sendmsg(io.file_descriptor, &io.message, 0);
    if(status< 0 && errno == EINVAL) {
        /* may something is wrong here */
        status = send(io.file_descriptor, buffer, length, 0);
        if(status < 0) {
            return -0xFEB;
        }
    }
    return status;
}

void VoiceServerSocket::network_event_write(int, short, void *ptr_network_events) {
    auto network_events = (NetworkEvents*) ptr_network_events;
    auto socket = network_events->socket;
    bool add_write_event{false};

    IOData<0x100> io{};
    io.file_descriptor = socket->file_descriptor;

    { /* write and process clients */
        std::shared_ptr<VoiceClient> client;
        protocol::OutgoingServerPacket* packet{nullptr};
        bool more_clients{true};

        auto write_timeout = system_clock::now() + microseconds(2500); /* read 2.5ms long at a time or 'till nothing more is there */
        while(system_clock::now() <= write_timeout) {
            more_clients = socket->pop_voice_write_queue(client);
            if(!client) {
                /* No pending client writes */
                break;
            }

            bool client_data_pending{true};
            while(client_data_pending && std::chrono::system_clock::now() <= write_timeout) {
                auto& client_packet_encoder = client->getConnection()->packet_encoder();

                assert(!packet);
                client_data_pending = client_packet_encoder.pop_write_buffer(packet);
                if(!packet) {
                    assert(!client_data_pending);
                    break;
                }

                ssize_t res = write_datagram(io, client->get_remote_address(), &client->getConnection()->remote_address_info(), packet->packet_length(), packet->packet_data());
                if(res <= 0) {
                    if(errno == EAGAIN) {
                        client_packet_encoder.reenqueue_failed_buffer(packet);
                        logTrace(socket->server_id, "Failed to write datagram packet for client {} (EAGAIN). Rescheduling packet.", client->getLoggingPeerIp() + ":" + to_string(client->getPeerPort()));
                        return;
                    } else if(errno == EINVAL || res == -0xFEB) {
                        /* needs more debug */
                        auto voice_client = dynamic_pointer_cast<VoiceClient>(client);
                        logCritical(
                                socket->server_id,
                                "Failed to write datagram packet ({} @ {}) for client {} ({}) {}. Dropping packet! Extra data: [fd: {}, client family: {}, socket family: {}]",
                                packet->packet_length(), packet->packet_data(),
                                client->getLoggingPeerIp() + ":" + to_string(client->getPeerPort()),
                                strerror(errno),
                                res,
                                socket->file_descriptor,
                                voice_client->isAddressV4() ? "v4" : voice_client->isAddressV6() ? "v6" : "v?",
                                socket->address_.ss_family == AF_INET ? "v4" : "v6"
                        );
                    } else {
                        logCritical(
                                socket->server_id,
                                "Failed to write datagram packet for client {} (errno: {} message: {}). Dropping packet!",
                                client->getLoggingPeerIp() + ":" + to_string(client->getPeerPort()),
                                errno,
                                strerror(errno)
                        );
                    }
                    packet->unref();
                    break;
                } else if(res != packet->packet_length()) {
                    logWarning(socket->server_id, "Datagram write result didn't matches the datagrams size. Expected {}, Received {}", packet->packet_length(), res);
                }

                packet->unref();
                packet = nullptr;
            }

            if(client_data_pending) {
                /* we exceeded the max write time, rescheduling write */
                socket->enqueue_client_write(client);
                more_clients = true;
            }

            client = nullptr;
        }

        add_write_event |= more_clients;
    }

    /* write all manually specified datagram packets */
    {
        auto write_timeout = system_clock::now() + std::chrono::microseconds{2500}; /* read 2.5ms long at a time or 'till nothing more is there */
        udp::DatagramPacket* packet;

        while(system_clock::now() <= write_timeout && (packet = socket->pop_dg_write_queue())) {
            ssize_t res = write_datagram(io, packet->address, &packet->pktinfo, packet->data_length, packet->data);
            if(res != packet->data_length) {
                if(errno == EAGAIN) {
                    /* Just resend it */
                    socket->send_datagram(packet);
                } else {
                    udp::DatagramPacket::destroy(packet);
                }

                logError(socket->server_id, "Failed to send datagram. Wrote {} out of {}. {}/{}", res, packet->data_length, errno, strerror(errno));
                add_write_event = false;
                break;
            }
            udp::DatagramPacket::destroy(packet);
        }

        add_write_event |= packet != nullptr; /* memory stored at packet is not accessible anymore. But anyways pop_dg_write_queue returns 0 if there is nothing more */
    }

    if(add_write_event) {
        event_add(network_events->event_write, nullptr);
    }
}


static union {
    char literal[8]{'T', 'S', '3', 'I', 'N', 'I', 'T', '1'};
    uint64_t integral;
} TS3INIT;

constexpr static auto kRecvBufferSize{1600}; //IPv6 MTU: 1500 | IPv4 MTU: 576
void VoiceServerSocket::network_event_read(int, short, void *ptr_network_events) {
    auto network_events = (NetworkEvents*) ptr_network_events;
    auto socket = network_events->socket;

    uint8_t raw_read_buffer[kRecvBufferSize]; //Allocate on stack, so we dont need heap here

    ssize_t bytes_read;
    pipes::buffer_view read_buffer{raw_read_buffer, kRecvBufferSize}; /* will not allocate anything, just sets its mode to ptr and that's it :) */

    sockaddr_storage remote_address{};
    iovec io_vector{};
    io_vector.iov_base = (void*) raw_read_buffer;
    io_vector.iov_len = kRecvBufferSize;

    char message_headers[0x100];

    msghdr message{};
    message.msg_name = &remote_address;
    message.msg_namelen = sizeof(remote_address);
    message.msg_iov = &io_vector;
    message.msg_iovlen = 1;
    message.msg_control = message_headers;
    message.msg_controllen = 0x100;

    auto read_timeout = system_clock::now() + microseconds{2500}; /* read 2.5ms long at a time or 'till nothing more is there */
    while(system_clock::now() <= read_timeout) {
        message.msg_flags = 0;
        bytes_read = recvmsg(socket->file_descriptor, &message, 0);

        if((message.msg_flags & MSG_TRUNC) > 0) {
            static std::chrono::system_clock::time_point last_error_message{};
            auto now = system_clock::now();
            if(last_error_message + std::chrono::seconds{5} < now) {
                logError(socket->server_id, "Received truncated message from {}", net::to_string(remote_address));
                last_error_message = now;
            }
            continue;
        }

        if(bytes_read < 0) {
            if(errno == EAGAIN) {
                break;
            }

            //Nothing more to read
            logCritical(socket->server_id, "Could not receive datagram packet! Code: {} Reason: {}", errno, strerror(errno));
            break;
        } else if(bytes_read == 0){
            /* We received a dara gram with zero length? Well, how the hell sends us such? */
            break;
        }

        if(bytes_read < 8) {
            /* every packet must be at least 8 bytes long... */
            continue;
        }

        if(*(uint64_t*) raw_read_buffer == TS3INIT.integral) {
            //Handle ddos protection...
            /* TODO: Don't pass the raw buffer instead pass the protocol::ClientPacketParser and ClientPacketParser mus allow the INIT packet */
            socket->server->pow_handler->handle_datagram(socket->shared_from_this(), remote_address, message, read_buffer.view(0, bytes_read));
            continue;
        }

        protocol::ClientPacketParser packet_parser{read_buffer.view(0, bytes_read)};
        if(!packet_parser.valid()) {
            return;
        }

        std::shared_ptr<VoiceClient> client{};
        {
            auto client_id = packet_parser.client_id();
            if(client_id > 0) {
                client = dynamic_pointer_cast<VoiceClient>(socket->server->server->find_client_by_id(client_id));
            } else {
                client = socket->server->findClient(&remote_address, true);
            }
        }

        if(!client) {
            continue;
        }

        auto client_connection = client->getConnection();
        if(memcmp(&client->get_remote_address(), &remote_address, sizeof(sockaddr_storage)) != 0) { /* verify the remote address */
            /* only encrypted packets are allowed */
            if(!packet_parser.has_flag(protocol::PacketFlag::Unencrypted) && client->connectionState() == ConnectionState::CONNECTED) {
                /* the ip had changed */
                if(client_connection->verify_encryption(packet_parser)) {
                    udp::pktinfo_storage remote_address_info;
                    udp::DatagramPacket::extract_info(message, remote_address_info);
                    socket->server->handleClientAddressChange(client, remote_address, remote_address_info);
                }
            } else {
                continue;
            }
        }

        if(client->connectionState() != ConnectionState::DISCONNECTED) {
            client_connection->handle_incoming_datagram(packet_parser);
        }
    }
}