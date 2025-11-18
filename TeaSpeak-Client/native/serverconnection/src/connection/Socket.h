#pragma once

#include <thread>
#include <event.h>
#include <memory>
#include <deque>
#include <mutex>

#include <pipes/buffer.h>
#include <functional>

#ifndef WIN32
    #include <netinet/in.h>
#else
    #include <WinSock2.h>
#endif

namespace tc::connection {
    class UDPSocket {
        public:
            explicit UDPSocket(const sockaddr_storage& /* target */);
            ~UDPSocket();

            const sockaddr_storage& remote_address() { return this->_remote_address; }

            bool initialize();
            void finalize();

            void send_message(const pipes::buffer_view& /* message */);

            /* Callbacks are called within the IO loop. Do not call any other functions except send_message! */
            std::function<void(const pipes::buffer_view& /* message */)> on_data;
            std::function<void(int /* error code */, int /* description */)> on_fatal_error;

            const std::thread& io_thread() { return this->_io_thread; }
        private:
            static void _io_execute(void *_ptr_socket, void *_ptr_event_base);
            static void _callback_read(evutil_socket_t, short, void*);
            static void _callback_write(evutil_socket_t, short, void*);

            void io_execute(void*);
            void callback_read(evutil_socket_t);
            void callback_write(evutil_socket_t);

            sockaddr_storage _remote_address;

            int file_descriptor = 0;

            std::recursive_mutex io_lock;
            std::thread _io_thread;
            event_base* io_base = nullptr;

            event* event_read = nullptr;
            event* event_write = nullptr;

            std::deque<pipes::buffer> write_queue;
    };
}