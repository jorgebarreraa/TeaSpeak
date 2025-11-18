//
// Created by wolverindev on 08.05.18.
//

#include <netinet/tcp.h>
#include <shared/src/crypt.h>
#include <shared/include/license/license.h>
#include <memory>
#include <misc/std_unique_ptr.h>
#include <ThreadPool/ThreadHelper.h>
#include "ServerConnection.h"

using namespace std;
using namespace std::chrono;
using namespace license;
using namespace license::manager;

ServerConnection::ServerConnection() {}
ServerConnection::~ServerConnection() {
	this->disconnect("deallocation");
	if(this->network.flush_thread)
		this->network.flush_thread->join();
}

#define CERR(message)                                   \
do {                                                    \
	FLERROR(this->listener.future_connect, message);    \
	return;                                             \
} while(0)

threads::Future<bool> ServerConnection::connect(const std::string &host, uint16_t port) {
	this->listener.future_connect = std::make_unique<threads::Future<bool>>();
	this->network.state = ConnectionState::CONNECTING;

	threads::Thread([&, host, port](){
		this->network.address_remote.sin_family = AF_INET;
		{
			auto address = gethostbyname(host.c_str());
			if(!address)
				CERR("invalid address");

			auto inet_address = (in_addr*) address->h_addr;
			if(!inet_address)
				CERR("invalid address (2)");
			this->network.address_remote.sin_addr.s_addr = inet_address->s_addr;
		}
		this->network.address_remote.sin_port = htons(port); //27786

		this->network.file_descriptor = socket(AF_INET, SOCK_STREAM, 0);
		if(this->network.file_descriptor < 0) CERR("Socket setup failed");
		if(::connect(this->network.file_descriptor, reinterpret_cast<const sockaddr *>(&this->network.address_remote), sizeof(this->network.address_remote)) < 0) CERR("connect() failed (" + to_string(errno) + " | " + strerror(errno) + ")");
		int enabled = 1, disabled = 0;
		if(setsockopt(this->network.file_descriptor, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) < 0); // CERR("could not set reuse addr");
		if(setsockopt(this->network.file_descriptor, IPPROTO_TCP, TCP_CORK, &disabled, sizeof(disabled)) < 0); // CERR("could not set no push");

		this->network.event_base = event_base_new();
		this->network.event_read = event_new(this->network.event_base, this->network.file_descriptor, EV_READ | EV_PERSIST, ServerConnection::handleEventRead, this);
		this->network.event_write = event_new(this->network.event_base, this->network.file_descriptor, EV_WRITE, ServerConnection::handleEventWrite, this);
		event_add(this->network.event_read, nullptr);

		this->network.event_base_dispatch = std::thread{[&]{
			event_base_dispatch(this->network.event_base);
			if(this->verbose)
				cout << "ev ended!" << endl;
		}};
		this->network.state = ConnectionState::CONNECTED;
		this->protocol.state = protocol::HANDSCHAKE;
		this->protocol.ping_thread = thread([&]{
			while(true) {
				{
					unique_lock lock(this->protocol.ping_lock);
					this->protocol.ping_notify.wait_until(lock, system_clock::now() + seconds(30), [&]{
						return this->network.state != ConnectionState::CONNECTED;
					});

					if(this->network.state != ConnectionState::CONNECTED) return;
				}

				this->ping();
			}
		});
		uint8_t handshakeBuffer[5];
		handshakeBuffer[0] = 0xC0;
		handshakeBuffer[1] = 0xFF;
		handshakeBuffer[2] = 0xEE;
		handshakeBuffer[3] = 2;
		handshakeBuffer[4] = 1; //Im a manager
		this->sendPacket(protocol::packet{protocol::PACKET_CLIENT_HANDSHAKE, string((const char*) handshakeBuffer, 5)}); //Initialise packet
	}).detach();

	return *this->listener.future_connect;
}

void ServerConnection::disconnect(const std::string& reason) {
	this->network.state = ConnectionState::DISCONNECTING;
	this->local_disconnect_message = reason;
	//TODO
	this->closeConnection();
}

#define F_ERROR_DISCONNECT(name) \
FLERROR(name, "connection closed locally" + (this->local_disconnect_message.empty() ? "" : " (" + this->local_disconnect_message + ")"))

void ServerConnection::closeConnection() {
	if(this->network.state == ConnectionState::UNCONNECTED) return;
	this->network.state = ConnectionState::DISCONNECTING;

	if(this->network.event_base_dispatch.get_id() == this_thread::get_id()) {
		this->network.flush_thread = new threads::Thread(THREAD_SAVE_OPERATIONS, [&](){ this->closeConnection(); });
		return;
	}

	if(this->network.event_write) {
		event_del(this->network.event_write);
		event_free(this->network.event_write);
		this->network.event_write = nullptr;
	}
	if(this->network.event_read) {
		event_del(this->network.event_read);
		event_free(this->network.event_read);
		this->network.event_read = nullptr;
	}
	if(this->network.event_base) {
		event_base_loopbreak(this->network.event_base);
		if(event_base_loopexit(this->network.event_base, nullptr) < 0) {
			if(this->verbose)
				cerr << "could not stop event loop!" << endl;
		}
	}
	threads::save_join(this->network.event_base_dispatch);
	if(this->network.event_base) {
		event_base_free(this->network.event_base);
		this->network.event_base = nullptr;
	}

	if(this->network.file_descriptor > 0) {
		shutdown(this->network.file_descriptor, SHUT_RDWR);
		this->network.file_descriptor = 0;
	}
	this->network.state = ConnectionState::UNCONNECTED;

	{
		this->protocol.ping_notify.notify_all();
		if(this->protocol.ping_thread.joinable())
			this->protocol.ping_thread.join();
	}

	F_ERROR_DISCONNECT(this->listener.future_register);
	F_ERROR_DISCONNECT(this->listener.future_connect);
	F_ERROR_DISCONNECT(this->listener.future_delete);
	F_ERROR_DISCONNECT(this->listener.future_list);
	F_ERROR_DISCONNECT(this->listener.future_login);
}

void ServerConnection::handleEventRead(int fd, short, void* _connection) {
	auto connection = (ServerConnection*) _connection;

	char buffer[1024];
	auto read = recv(fd, buffer, 1024, MSG_DONTWAIT);
	if(read <= 0) {
		if(connection->verbose)
			cout << "Invalid read: " << strerror(errno) << endl;
		connection->local_disconnect_message = "invalid read";
		connection->closeConnection();
		return;
	}
	if(read > 0) {
		if(connection->verbose)
			cout << "Read: " << read << endl;
		connection->local_disconnect_message = "invalid read";
		connection->handleMessage(string(buffer, read));
	}
}

void ServerConnection::handleEventWrite(int fd, short, void* _connection) {
	auto connection = (ServerConnection*) _connection;

	threads::MutexLock lock(connection->network.queue_lock);
	auto& queue = connection->network.queue_write;
	if(queue.empty()) return;
	auto message = queue.front();
	queue.pop_front();

	auto wrote = send(fd, message.data(), message.length(), 0);
	if(wrote < 0) {
		if(connection->verbose)
			cout << "Invalid write: " << strerror(errno) << endl;
		connection->local_disconnect_message = "invalid write";
		connection->closeConnection();
		return;
	}
	if(connection->verbose)
		cout << "Wrote: " << wrote << endl;

	if(wrote < message.length()) {
		queue.push_front(message.substr(wrote));
		event_add(connection->network.event_write, nullptr);
	}
}

void ServerConnection::sendPacket(const protocol::packet& packet) {
	if(this->network.state == ConnectionState::UNCONNECTED || this->network.state == ConnectionState::DISCONNECTING) {
		if(this->verbose)
			cout << "Tried to send a packet to an unconnected remote!" << endl;
		return;
	}
	packet.prepare();

	string buffer;
	buffer.resize(packet.data.length() + sizeof(packet.header));
	memcpy((void*) buffer.data(), &packet.header, sizeof(packet.header));
	memcpy((void*) &buffer.data()[sizeof(packet.header)], packet.data.data(), packet.data.length());

	if(!this->protocol.crypt_key.empty())
		xorBuffer(&buffer[sizeof(packet.header)], packet.data.length(), this->protocol.crypt_key.data(), this->protocol.crypt_key.length());

	{
		threads::MutexLock lock(this->network.queue_lock);
		this->network.queue_write.push_back(buffer);
	}
	event_add(this->network.event_write, nullptr);
}