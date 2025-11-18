#include "./resolver.h"
#include "./response.h"

#include <cassert>
#include <functional>
#include <event.h>
#include <iostream>
#include <cstring>
#include <utility>

#include <fcntl.h> /* for TSDNS */
#ifdef WIN32
	#include <ws2tcpip.h>
    #define SOCK_NONBLOCK (0)
    #define MSG_DONTWAIT (0)
#else
    #include <unistd.h>
	#include <sys/socket.h>
#endif

using namespace std;
using namespace tc::dns;

Resolver::Resolver() { }

Resolver::~Resolver() {
	this->finalize();
}


bool Resolver::initialize(std::string &error, bool hosts, bool resolv) {
    if(this->event.loop_active)
        this->finalize();

    this->event.loop_active = true;
    this->event.base = event_base_new();
    if(!this->event.base) {
        error = "failed to allcoate event base";
        return false;
    }
    this->event.loop = std::thread(std::bind(&Resolver::event_loop_runner, this));

    return this->initialize_platform(error, hosts, resolv);
}

void Resolver::finalize() {
    this->event.loop_active = false;
    if(this->event.base) {
        auto ret = event_base_loopexit(this->event.base, nullptr);
        if(ret != 0) {
            cerr << "Failed to exit event base loop: " << ret << endl;
        }
    }

    {
        this->event.condition.notify_one();
        if(this->event.loop.joinable())
            this->event.loop.join();
    }

    {
        unique_lock lock(this->request_lock);
        auto dns_list = std::move(this->dns_requests);
        auto tsdns_list = std::move(this->tsdns_requests);

        for(auto entry : dns_list) {
            entry->callback(ResultState::ABORT, 0, nullptr);
            this->destroy_dns_request(entry);
        }

        for(auto entry : tsdns_list) {
            entry->callback(ResultState::ABORT, 0, "");
            this->destroy_tsdns_request(entry);
        }
        lock.unlock();
    }

    this->finalize_platform(); /* keep the event base allocated until platform depend finalize has been done */
    if(this->event.base) {
        event_base_free(this->event.base);
        this->event.base = nullptr;
    }
}

void Resolver::event_loop_runner() {
    while(true) {
        {
            unique_lock lock{this->event.lock};
            if(!this->event.loop_active)
                break;

            this->event.condition.wait(lock);
            if(!this->event.loop_active)
                break;
        }

        event_base_loop(this->event.base, 0);
    }
}

void Resolver::destroy_tsdns_request(Resolver::tsdns_request *request) {
	assert(this_thread::get_id() == this->event.loop.get_id() || !this->event.loop_active);

	{
		lock_guard lock{this->request_lock};
		this->tsdns_requests.erase(std::find(this->tsdns_requests.begin(), this->tsdns_requests.end(), request), this->tsdns_requests.end());
	}

	if(request->event_read) {
		event_del_noblock(request->event_read);
		event_free(request->event_read);
		request->event_read = nullptr;
	}

	if(request->event_write) {
		event_del_noblock(request->event_write);
		event_free(request->event_write);
		request->event_write = nullptr;
	}

	if(request->timeout_event) {
		event_del_noblock(request->timeout_event);
		event_free(request->timeout_event);
		request->timeout_event = nullptr;
	}

	if(request->socket > 0) {
#ifndef WIN32
		::shutdown(request->socket, SHUT_RDWR);
		::close(request->socket);
#else
        closesocket(request->socket);
#endif
		request->socket = 0;
	}

	delete request;
}

//---------------------- TSDNS
void Resolver::resolve_tsdns(const char *query, const sockaddr_storage& server_address, const std::chrono::microseconds& timeout, const tc::dns::Resolver::tsdns_callback_t &callback) {
	/* create the socket */
	auto socket = ::socket(server_address.ss_family, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if(socket <= 0) {
#ifdef WIN32
	    char buf[1024];
	    strerror_s(buf, errno);
	    std::string strerr{buf};
#else
        std::string strerr{strerror(errno)};
#endif
		callback(ResultState::INITIALISATION_FAILED, -1, "failed to allocate socket: " + to_string(errno) + "/" + strerr);
		return;
	}

#ifdef WIN32
	u_long enabled = 0;
    auto non_block_rs = ioctlsocket(socket, FIONBIO, &enabled);
    if (non_block_rs != NO_ERROR) {
#ifdef WIN32
        char buf[1024];
        strerror_s(buf, errno);
        std::string strerr{buf};
#else
        std::string strerr{strerror(errno)};
#endif
        closesocket(socket);
        callback(ResultState::INITIALISATION_FAILED, -2, "failed to enable nonblock: " + to_string(errno) + "/" + strerr);
		return;
    }
#else
    int opt = 1;
	setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
#endif

	auto request = new tsdns_request{};

	request->resolver = this;
	request->callback = callback;
	request->socket = (int) socket;
	request->timeout_event = evtimer_new(this->event.base, [](evutil_socket_t, short, void *_request) {
		auto request = static_cast<tsdns_request*>(_request);
		request->resolver->evtimer_tsdns_callback(request);
	}, request);

	request->event_read = event_new(this->event.base, socket, EV_READ | EV_PERSIST, [](evutil_socket_t, short, void *_request){
		auto request = static_cast<tsdns_request*>(_request);
		request->resolver->event_tsdns_read(request);
	}, request);
	request->event_write = event_new(this->event.base, socket, EV_WRITE, [](evutil_socket_t, short, void *_request){
		auto request = static_cast<tsdns_request*>(_request);
		request->resolver->event_tsdns_write(request);
	}, request);

	if(!request->timeout_event || !request->event_write || !request->event_read) {
		callback(ResultState::INITIALISATION_FAILED, -3, "");
		this->destroy_tsdns_request(request);
		return;
	}

	request->write_buffer = query;
	request->write_buffer += "\n\r\r\r\n";

	int result = ::connect(socket, reinterpret_cast<const sockaddr *> (&server_address), sizeof(server_address));
	if (result < 0) {
#ifdef WIN32
		auto error = WSAGetLastError();

        if(error != WSAEWOULDBLOCK) {
            char* s = nullptr;
            FormatMessageA(
                    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    nullptr,
                    error,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    (LPSTR) &s,
                    0,
                    nullptr
            );

            std::string message{s};
            LocalFree(s);

			callback(ResultState::TSDNS_CONNECTION_FAIL, -1, "Failed to connect: " + message);
			this->destroy_tsdns_request(request);
			return;
        }
#else
		if(errno != EINPROGRESS) {
			callback(ResultState::TSDNS_CONNECTION_FAIL, -1, "Failed to connect with code: " + to_string(errno) + "/" + strerror(errno));
			this->destroy_tsdns_request(request);
			return;
		}
#endif
	}

	{
		auto seconds = chrono::floor<chrono::seconds>(timeout);
		auto microseconds = chrono::ceil<chrono::microseconds>(timeout - seconds);

		timeval tv{(long) seconds.count(), (long) microseconds.count()};
		auto errc = event_add(request->timeout_event, &tv);

		//TODO: Check for error
	}

	{
		lock_guard lock{this->request_lock};
		this->tsdns_requests.push_back(request);
	}

    event_add(request->event_write, nullptr);
    event_add(request->event_read, nullptr);

	/* Activate the event loop */
	this->event.condition.notify_one();
}

void Resolver::evtimer_tsdns_callback(Resolver::tsdns_request *request) {
	request->callback(ResultState::DNS_TIMEOUT, 0, "");
	this->destroy_tsdns_request(request);
}

void Resolver::event_tsdns_read(Resolver::tsdns_request *request) {
	int64_t buffer_length = 1024;
	char buffer[1024];

	buffer_length = recv(request->socket, buffer, (int) buffer_length, MSG_DONTWAIT);
	if(buffer_length < 0) {
#ifdef WIN32
		auto error = WSAGetLastError();
        if(error == WSAEWOULDBLOCK) {
            return;
        }
		request->callback(ResultState::TSDNS_CONNECTION_FAIL, -2, "read failed: " + to_string(error));
#else
		if(errno == EAGAIN) {
			return;
		}
		request->callback(ResultState::TSDNS_CONNECTION_FAIL, -2, "read failed: " + to_string(errno) + "/" + strerror(errno));
#endif
		this->destroy_tsdns_request(request);
		return;
	} else if(buffer_length == 0) {
		if(request->read_buffer.empty() || request->read_buffer == "404") {
			request->callback(ResultState::TSDNS_EMPTY_RESPONSE, 0, "");
		} else {
			request->callback(ResultState::SUCCESS, 0, request->read_buffer);
		}
		this->destroy_tsdns_request(request);
		return;
	}

	lock_guard lock{request->buffer_lock};
	request->read_buffer.append(buffer, buffer_length);
}

void Resolver::event_tsdns_write(Resolver::tsdns_request *request) {
	lock_guard lock{request->buffer_lock};
	if(request->write_buffer.empty())
		return;

	auto written = send(request->socket, request->write_buffer.data(), (int) min(request->write_buffer.size(), 1024UL), MSG_DONTWAIT);
	if(written < 0) {
#ifdef WIN32
		auto error = WSAGetLastError();
        if(error != WSAEWOULDBLOCK)
            return;
		request->callback(ResultState::TSDNS_CONNECTION_FAIL, -4, "write failed: " + to_string(error));
#else
		if(errno == EAGAIN)
			return;
		request->callback(ResultState::TSDNS_CONNECTION_FAIL, -4, "write failed: " + to_string(errno) + "/" + strerror(errno));
#endif
		this->destroy_tsdns_request(request);
		return;
	} else if(written == 0) {
		request->callback(ResultState::TSDNS_CONNECTION_FAIL, -5, "remote peer hang up");
		this->destroy_tsdns_request(request);
		return;
	}

	if(written == request->write_buffer.size())
		request->write_buffer.clear();
	else {
		request->write_buffer = request->write_buffer.substr(written);
		event_add(request->event_write, nullptr);
	}
}