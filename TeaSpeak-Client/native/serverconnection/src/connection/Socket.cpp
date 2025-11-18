#include "Socket.h"
#include "../logger.h"
#include <thread>
#include <cstring>
#include <string>
#include <iostream>

#ifdef WIN32
    #include <WinSock2.h>
    #define SOCK_NONBLOCK (0)
    #define MSG_DONTWAIT (0)
    typedef int socklen_t;
#else
    #include <unistd.h>
	#include <netinet/ip.h>
#endif

using namespace std;
using namespace tc::connection;

UDPSocket::UDPSocket(const sockaddr_storage &address) {
	memcpy(&this->_remote_address, &address, sizeof(sockaddr_storage));
}

UDPSocket::~UDPSocket() {
	this->finalize();
}

bool UDPSocket::initialize() {
	if(this->file_descriptor > 0)
		return false;

	this->file_descriptor = (int) socket(this->_remote_address.ss_family, SOCK_DGRAM | SOCK_NONBLOCK, 0);
	if(this->file_descriptor < 2) {
		this->file_descriptor = 0;
		return false;
	}

#ifdef WIN32
	u_long enabled = 1;
    auto non_block_rs = ioctlsocket(this->file_descriptor, FIONBIO, &enabled);
    if (non_block_rs != NO_ERROR) {
		log_warn(category::connection, tr("Failed to enable noblock!"));
    }
#endif

	/*
	 * TODO: Make configurable
	 */
	//uint8_t value = IPTOS_DSCP_EF;
	//if(setsockopt(this->file_descriptor, IPPROTO_IP, IP_TOS, &value, sizeof(value)) < 0)
	//	log_warn(category::connection, "Failed to set TOS high priority on socket");

	this->io_base = event_base_new();
	if(!this->io_base) { /* may too many file descriptors already open */
		this->finalize();
		return false;
	}
	this->event_read = event_new(this->io_base, this->file_descriptor, EV_READ | EV_PERSIST, &UDPSocket::_callback_read, this);
	this->event_write = event_new(this->io_base, this->file_descriptor, EV_WRITE, &UDPSocket::_callback_write, this);
	event_add(this->event_read, nullptr);

	this->_io_thread = thread(&UDPSocket::_io_execute, this, this->io_base);
#ifdef WIN32
    //TODO set thread name
#else
	auto handle = this->_io_thread.native_handle();
	pthread_setname_np(handle, "UDPSocket loop");
#endif
	return true;
}

void UDPSocket::finalize() {
    const auto is_event_thread = this_thread::get_id() == this->_io_thread.get_id();
	if(this->file_descriptor == 0)
		return;

	unique_lock lock(this->io_lock);
	auto ev_read = std::exchange(this->event_read, nullptr);
	auto ev_write = std::exchange(this->event_write, nullptr);
	auto io_base_loop = std::exchange(this->io_base, nullptr);
	lock.unlock();

	if(is_event_thread) {
        if(ev_read) event_del_block(ev_read);
        if(ev_write) event_del_block(ev_write);
	} else {
        if(ev_read) event_del_noblock(ev_read);
        if(ev_write) event_del_noblock(ev_write);
    }

	if(io_base_loop) {
        event_base_loopexit(io_base_loop, nullptr);
	}

	if(is_event_thread) {
        this->_io_thread.detach();
	} else {
        if(this->_io_thread.joinable())
            this->_io_thread.join();
    }

#ifdef WIN32
	const auto close_result = ::closesocket(this->file_descriptor);
#else
    const auto close_result = ::close(this->file_descriptor);
#endif
    if(close_result != 0) {
		if(errno != EBADF)
			logger::warn(category::socket, tr("Failed to close file descriptor ({}/{})"), to_string(errno), strerror(errno));
	}
	this->file_descriptor = 0;
}

void UDPSocket::_callback_write(evutil_socket_t fd, short, void *_ptr_socket) {
	((UDPSocket*) _ptr_socket)->callback_write(fd);
}

void UDPSocket::_callback_read(evutil_socket_t fd, short, void *_ptr_socket) {
	((UDPSocket*) _ptr_socket)->callback_read(fd);
}

void UDPSocket::_io_execute(void *_ptr_socket, void* _ptr_event_base) {
	((UDPSocket*) _ptr_socket)->io_execute(_ptr_event_base);
}

void UDPSocket::io_execute(void* ptr_event_base) {
    auto base = (event_base*) ptr_event_base;
	event_base_loop(base, EVLOOP_NO_EXIT_ON_EMPTY);
	/* this pointer might be dangling here! */
	logger::trace(category::socket, tr("Socket IO loop exited"));
    event_base_free(base);
}
void UDPSocket::callback_read(evutil_socket_t fd) {
	sockaddr_storage source_address{};
	socklen_t source_address_length;

	ssize_t read_length = -1;
	size_t buffer_length = 1600; /* IPv6 MTU is ~1.5k */
	char buffer[1600];

    source_address_length = sizeof(sockaddr_storage);
    read_length = recvfrom(fd, (char*) buffer, (int) buffer_length, MSG_DONTWAIT, (sockaddr*) &source_address, &source_address_length);
    if(read_length <= 0) {
        int error;
#ifdef WIN32
        error = WSAGetLastError();
        if(error == WSAEWOULDBLOCK)
            return;
#else
        error = errno;
        if(errno == EAGAIN)
            return;
#endif
        logger::warn(category::socket, tr("Failed to receive data: {}"), error);
        {
            std::lock_guard lock{this->io_lock};
            if(this->event_read)
                event_del_noblock(this->event_read);
        }

        if(auto callback{this->on_fatal_error}; callback)
            callback(1, error);
        /* this pointer might be dangling now because we got deleted while handling this data */
        return;
    }

    //logger::trace(category::socket, tr("Read {} bytes"), read_length);
    if(this->on_data)
        this->on_data(pipes::buffer_view{buffer, (size_t) read_length});
    /* this pointer might be dangling now because we got deleted while handling this data */
}

void UDPSocket::callback_write(evutil_socket_t fd) {
	unique_lock lock(this->io_lock);
	if(this->write_queue.empty())
		return;

	auto buffer = this->write_queue.front();
	this->write_queue.pop_front();
	lock.unlock();

	auto written = sendto(fd, buffer.data_ptr<char>(), (int) buffer.length(), MSG_DONTWAIT, (sockaddr*) &this->_remote_address, sizeof(this->_remote_address));
	if(written != buffer.length()) {
        int error;
#ifdef WIN32
        if(error = WSAGetLastError(); error == WSAEWOULDBLOCK) {
#else
        if(error = errno; errno == EAGAIN) {
#endif
			lock.lock();
			this->write_queue.push_front(buffer);
			if(this->event_write)
				event_add(this->event_write, nullptr);
			return;
		}

        logger::warn(category::socket, tr("Failed to send data: {}"), error);
        if(auto callback{this->on_fatal_error}; callback)
            callback(2, error);
		return; /* this should never happen! */
	} else {
        //logger::trace(category::socket, tr("Wrote {} bytes"), buffer.length());
    }

	lock.lock();
	if(!this->write_queue.empty() && this->event_write)
		event_add(this->event_write, nullptr);
}

void UDPSocket::send_message(const pipes::buffer_view &buffer) {
	auto buf = buffer.own_buffer();

	unique_lock lock(this->io_lock);
	this->write_queue.push_back(buf);
	if(this->event_write) {
        event_add(this->event_write, nullptr);
	} else {
	    logger::warn(category::socket, tr("Dropping write event schedule because we have no write event."));
    }
}