#include <unistd.h>
#include <csignal>
#include <sys/socket.h>
#include <netinet/in.h>
#include <algorithm>
#include <utility>
#include <arpa/inet.h>
#include <log/LogUtils.h>
#include <misc/endianness.h>
#include <LicenseRequest.pb.h>
#include <shared/include/license/license.h>
#include <shared/src/crypt.h>
#include <ThreadPool/ThreadHelper.h>
#include "LicenseServer.h"
#include "UserManager.h"

using namespace std;
using namespace std::chrono;
using namespace license;
using namespace ts;

LicenseServer::LicenseServer(const sockaddr_in& addr,
        std::shared_ptr<server::database::DatabaseHandler>  manager,
        shared_ptr<license::stats::StatisticManager> stats,
        shared_ptr<license::web::WebStatistics> wstats,
        std::shared_ptr<UserManager>  user_manager) : manager{std::move(manager)}, statistics{std::move(stats)}, web_statistics{std::move(wstats)}, user_manager{std::move(user_manager)} {
    memcpy(&this->localAddr, &addr, sizeof(addr));
}

LicenseServer::~LicenseServer() {
    this->stop();
}

#define SFAIL(message)                                                                          \
do {                                                                                            \
    logError(LOG_GENERAL, " Message: {} ({}/{})", message, errno, strerror(errno));             \
    this->stop();                                                                         \
    return false;                                                                               \
} while(0)

static int enabled = 1;
static int disabled = 0;
bool LicenseServer::start() {
    {
        lock_guard lock(this->client_lock);
        if(this->running) return false;
        this->running = true;
    }

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) SFAIL("Could not create new socket");

    if(setsockopt(this->server_socket, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) < 0) SFAIL("could not set reuse address");
    if(setsockopt(this->server_socket, IPPROTO_TCP, TCP_CORK, &disabled, sizeof(disabled)) < 0) SFAIL("could not set no push");
    if(bind(this->server_socket, (struct sockaddr *) &this->localAddr, sizeof(sockaddr_in)) < 0) SFAIL("Could not bind socket on " + string(inet_ntoa(this->localAddr.sin_addr)));

    if(listen(this->server_socket, 32) < 0) SFAIL("Could not listen on socket");

    this->evBase = event_base_new();
    this->event_accept = event_new(this->evBase, this->server_socket, EV_READ | EV_PERSIST, LicenseServer::handleEventAccept, this);
    this->event_cleanup = evtimer_new(this->evBase, LicenseServer::handleEventCleanup, this);

    event_add(this->event_accept, nullptr);
    {
        timeval now{1, 0};
        evtimer_add(this->event_cleanup, &now);
    }

    event_base_dispatch = std::thread([&]{
        signal(SIGPIPE, SIG_IGN);
        ::event_base_dispatch(this->evBase);
    });
    return true;
}

void LicenseServer::stop() {
    {
        lock_guard lock(this->client_lock);
        if(!this->running) return;
        this->running = false;
    }

    /* first unregister the accept event so we don't get new clients */
    if(this->event_accept) {
        event_del(this->event_accept);
        event_free(this->event_accept);
    }
    this->event_accept = nullptr;

    /* disconnect all clients */
    for(const auto& client : this->getClients())
        this->closeConnection(client);

    if(this->evBase)
        event_base_loopbreak(this->evBase);

    if(!threads::timed_join(this->event_base_dispatch, std::chrono::seconds{2})) {
        this->event_base_dispatch.detach();
        logCritical(LOG_GENERAL, "Failed to join event base dispatch thread. This will cause memory leaks.");
    }

    /* Needs to be cleaned up after event loop has been destroyed. Because its used within the event loop. */
    if(this->event_cleanup) {
        event_del_block(this->event_cleanup);
        event_free(this->event_cleanup);
        this->event_cleanup = nullptr;
    }

	if(this->evBase)
		event_base_free(this->evBase);
	this->evBase = nullptr;

    if(this->server_socket != 0) {
        shutdown(this->server_socket, SHUT_RDWR);
        close(this->server_socket);
        this->server_socket = 0;
    }
}

void LicenseServer::handleEventCleanup(int, short, void* ptrServer) {
    auto server = static_cast<LicenseServer *>(ptrServer);

    server->cleanup_clients();
    timeval next{1, 0};

    if(server->event_cleanup)
        event_add(server->event_cleanup, &next);
}

//Basic IO
void LicenseServer::handleEventWrite(int fd, short, void* ptrServer) {
    auto server = static_cast<LicenseServer *>(ptrServer);
    auto client = server->findClient(fd);
    if(!client) return;

    buffer::RawBuffer* write_buffer{nullptr};
    std::unique_lock write_lock(client->network.write_queue_lock);
    while(true) { //TODO: May add some kind of timeout?
        write_buffer = TAILQ_FIRST(&client->network.write_queue);
        if(!write_buffer) return;

        auto writtenBytes = send(fd, &write_buffer->buffer[write_buffer->index], write_buffer->length - write_buffer->index, 0);
        if(writtenBytes <= 0) {
            write_lock.unlock();
        	if(writtenBytes == -1 && errno == EAGAIN)
        		return;
	        logError(LOG_LICENSE_CONTROLL, "Invalid write. Disconnecting remote client. Message: {}/{}", errno, strerror(errno));
	        server->unregisterClient(client);
	        return;
        } else {
            write_buffer->index += writtenBytes;
        }

        if(write_buffer->index >= write_buffer->length) {
            TAILQ_REMOVE(&client->network.write_queue, write_buffer, tail);
            delete write_buffer;
        }
        if(!TAILQ_EMPTY(&client->network.write_queue))
            event_add(client->network.writeEvent, nullptr);
    }
}

void ConnectedClient::sendPacket(const protocol::packet& packet) {
	packet.prepare();

    auto buffer = new buffer::RawBuffer(packet.data.length() + sizeof(packet.header));
    memcpy(buffer->buffer, &packet.header, sizeof(packet.header));
    memcpy(&buffer->buffer[sizeof(packet.header)], packet.data.data(), packet.data.length());

    if(!this->protocol.cryptKey.empty())
        xorBuffer(&buffer->buffer[sizeof(packet.header)], packet.data.length(), this->protocol.cryptKey.data(), this->protocol.cryptKey.length());

    {
        lock_guard queue_lock{this->network.write_queue_lock};
        TAILQ_INSERT_TAIL(&this->network.write_queue, buffer, tail);
    }
    {
        lock_guard state_lock{this->protocol.state_lock};
        if(this->protocol.state == protocol::UNCONNECTED) goto error_cleanup;

        event_add(this->network.writeEvent, nullptr);
        return;
    }
    error_cleanup:
    delete buffer;
}

void ConnectedClient::init() {
	protocol.last_read = std::chrono::system_clock::now();
	TAILQ_INIT(&network.write_queue);
}

void ConnectedClient::uninit(bool blocking) {
    {
        lock_guard queue_lock{this->network.write_queue_lock};
        ts::buffer::RawBuffer* buffer;
        while ((buffer = TAILQ_FIRST(&this->network.write_queue))) {
            TAILQ_REMOVE(&this->network.write_queue, buffer, tail);
            delete buffer;
        }
    }
    if(network.fileDescriptor > 0) {
        shutdown(this->network.fileDescriptor, SHUT_RDWR);
        close(this->network.fileDescriptor);
	    network.fileDescriptor = 0;
    }

    std::unique_lock elock{this->network.event_mutex};
    auto read_event = std::exchange(this->network.readEvent, nullptr);
    auto write_event = std::exchange(this->network.writeEvent, nullptr);
    elock.unlock();

    if(blocking) {
        if(read_event) event_del_block(read_event);
        if(write_event) event_del_block(write_event);
    } else {
        if(read_event) event_del_noblock(read_event);
        if(write_event) event_del_noblock(write_event);
    }
    if(read_event) event_free(read_event);
    if(write_event) event_free(write_event);
}

void LicenseServer::handleEventRead(int fd, short, void* ptrServer) {
    auto server = static_cast<LicenseServer *>(ptrServer);
    auto client = server->findClient(fd);
    if(!client) return;

    char buffer[1024];
    sockaddr_in remoteAddress{};
    socklen_t remoteAddressSize = sizeof(remoteAddress);
    auto read = recvfrom(fd, buffer, 1024, 0, reinterpret_cast<sockaddr *>(&remoteAddress), &remoteAddressSize);

    if(read < 0){
        if(errno == EWOULDBLOCK) return;
        logError(LOG_LICENSE_CONTROLL, "Invalid read. Disconnecting remote client. Message: {}/{}", errno, strerror(errno));
        {
            std::lock_guard elock{client->network.event_mutex};
            if(client->network.readEvent)
                event_del_noblock(client->network.readEvent);
        }
	    server->closeConnection(client);
        return;
    } else if(read == 0) {
        logMessage(LOG_LICENSE_CONTROLL, "[CLIENT][" + client->address() + "] Received EOF for client. Removing client.");
        {
            std::lock_guard elock{client->network.event_mutex};
            if(client->network.readEvent)
                event_del_noblock(client->network.readEvent);
        }
	    server->closeConnection(client);
        return;
    }

    client->protocol.last_read = std::chrono::system_clock::now();
    server->handleMessage(client, string(buffer, read));
}

void LicenseServer::handleEventAccept(int fd, short, void* ptrServer) {
    auto server = static_cast<LicenseServer *>(ptrServer);

    auto client = make_shared<ConnectedClient>();
    client->init();

    socklen_t client_len = sizeof(client->network.remoteAddr);

    client->network.fileDescriptor = accept(fd, (struct sockaddr *)&client->network.remoteAddr, &client_len);
    if(setsockopt(client->network.fileDescriptor, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) < 0);// CERR("could not set reuse addr");
    if(setsockopt(client->network.fileDescriptor, IPPROTO_TCP, TCP_CORK, &disabled, sizeof(disabled)) < 0);// CERR("could not set no push");

    if (client->network.fileDescriptor < 0) {
        logCritical(LOG_GENERAL, "Could not accept new client! (" + to_string(client->network.fileDescriptor) + "|" + to_string(errno) + "|" + strerror(errno) + ")");
        return;
    }

    client->protocol.state = protocol::HANDSCHAKE;
    {
        lock_guard lock(server->client_lock);
        server->clients.push_back(client);
    }

    client->network.readEvent = event_new(server->evBase, client->network.fileDescriptor, EV_READ | EV_PERSIST, LicenseServer::handleEventRead, server);
    client->network.writeEvent = event_new(server->evBase, client->network.fileDescriptor, EV_WRITE, LicenseServer::handleEventWrite, server);
    event_add(client->network.readEvent, nullptr);

    logMessage(LOG_GENERAL, "Accepted new client from {}", inet_ntoa(client->network.remoteAddr.sin_addr));
}

void LicenseServer::disconnectClient(const std::shared_ptr<ConnectedClient>& client, const std::string &reason) {
	client->sendPacket({protocol::PACKET_DISCONNECT, reason});
}

void LicenseServer::closeConnection(const std::shared_ptr<ConnectedClient> &client, bool blocking) {
    if(this_thread::get_id() == this->event_base_dispatch.get_id()) {
        std::thread(std::bind(&LicenseServer::closeConnection, this, client, true)).detach();
        return;
    }

    {

        unique_lock lock(client->network.write_queue_lock);
        if(!TAILQ_EMPTY(&client->network.write_queue)) {
            lock.unlock();

            if(!blocking) {
                std::thread(std::bind(&LicenseServer::closeConnection, this, client, true)).detach();
                return;
            }
            auto start = system_clock::now();
            while(system_clock::now() - start < seconds(5)){
                {
                    lock.lock();
                    if(TAILQ_EMPTY(&client->network.write_queue)) break;
                    lock.unlock();
                }
                threads::self::sleep_for(milliseconds(5));
            }
        }
    }
    this->unregisterClient(client);
}

void LicenseServer::unregisterClient(const std::shared_ptr<ConnectedClient> &client) {
    {
        lock_guard lock(this->client_lock);

        auto it = find(this->clients.begin(), this->clients.end(), client);
        if(it != this->clients.end())
            this->clients.erase(it);
    }

    {
        std::lock_guard state_lock{client->protocol.state_lock};
        client->protocol.state = protocol::UNCONNECTED;
    }
    client->uninit(this_thread::get_id() != this->event_base_dispatch.get_id());
}

void LicenseServer::cleanup_clients() {
    unique_lock lock(this->client_lock);
    auto clients = this->clients;
    lock.unlock();

    size_t cleanup_count{0};
    for(const auto& client : clients) {
        if(client->protocol.last_read + minutes(1) < system_clock::now()) {
            cleanup_count++;
            if(client->protocol.state != protocol::DISCONNECTING && client->protocol.state != protocol::UNCONNECTED) {
                this->disconnectClient(client, "timeout");
                this->closeConnection(client);

                std::lock_guard state_lock{client->protocol.state_lock};
                client->protocol.state = protocol::UNCONNECTED;
            } else {
                auto it = find(this->clients.begin(), this->clients.end(), client);
                if(it != this->clients.end())
                    this->clients.erase(it);
            }
        }
    }
    if(cleanup_count)
        debugMessage(LOG_GENERAL, "{} clients have been cleaned up due to a read timeout.", cleanup_count);
}

std::shared_ptr<ConnectedClient> LicenseServer::findClient(int fd) {
    lock_guard lock(this->client_lock);
    for(const auto& cl : this->clients)
        if(cl->network.fileDescriptor == fd)
            return cl;
    return nullptr;
}

#define ERR(message)                            \
do {                                            \
    logError(LOG_GENERAL, message);               \
    this->closeConnection(client);              \
    return;                                     \
} while(0)

void LicenseServer::handleMessage(shared_ptr<ConnectedClient>& client, const std::string& message) {
    if(message.length() < sizeof(protocol::packet::header)) ERR("A client tried to send a invalid packet (too small header)");

    protocol::packet packet{protocol::PACKET_DISCONNECT, ""};
    memcpy(&packet.header, message.data(), sizeof(protocol::packet::header));
    packet.data = message.substr(sizeof(protocol::packet::header), packet.header.length);

    if(!client->protocol.cryptKey.empty()) {
        xorBuffer((char*) packet.data.data(), packet.data.length(), client->protocol.cryptKey.data(), client->protocol.cryptKey.length());
    }

	bool success{false};
	string error;
	try {
		if(packet.header.packetId == protocol::PACKET_CLIENT_HANDSHAKE) {
			success = this->handleHandshake(client, packet, error);
		} else if(packet.header.packetId == protocol::PACKET_DISCONNECT) {
			success = this->handleDisconnect(client, packet, error);
		} else if(packet.header.packetId == protocol::PACKET_CLIENT_SERVER_VALIDATION) {
			success = this->handleServerValidation(client, packet, error);
		} else if(packet.header.packetId == protocol::PACKET_CLIENT_PROPERTY_ADJUSTMENT) {
			success = this->handlePacketPropertyUpdate(client, packet, error);
		} else if(packet.header.packetId == protocol::PACKET_CLIENT_LICENSE_UPGRADE) {
		    success = this->handlePacketLicenseUpgrade(client, packet, error);
		} else if(packet.header.packetId == protocol::PACKET_CLIENT_AUTH_REQUEST) {
			success = this->handlePacketAuth(client, packet, error);
		} else if(packet.header.packetId == protocol::PACKET_CLIENT_LICENSE_CREATE_REQUEST) {
			success = this->handlePacketLicenseCreate(client, packet, error);
		} else if(packet.header.packetId == protocol::PACKET_CLIENT_LIST_REQUEST) {
			success = this->handlePacketLicenseList(client, packet, error);
        } else if(packet.header.packetId == protocol::PACKET_CLIENT_DELETE_REQUEST) {
			success = this->handlePacketLicenseDelete(client, packet, error);
        } else if(packet.header.packetId == protocol::PACKET_PING) {
		    /* nothing todo */
		} else error = "Invalid packet id!";
	} catch (std::exception& ex) {
		success = false;
		error = "Caught exception while handle packet: " + string(ex.what());
	}

	if(!success) {
		logError(LOG_GENERAL, "[CLIENT][" + client->address() + "] Failed to handle packet. message: " + error);
		this->disconnectClient(client, error);
	}
}