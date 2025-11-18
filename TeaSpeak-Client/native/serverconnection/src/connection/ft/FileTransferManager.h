#pragma once

#include <deque>
#include <mutex>
#include <memory>
#include <utility>
#include <cstdio>
#include <string>
#include <thread>
#include <functional>
#include <condition_variable>

#include <event.h>
#include <pipes/buffer.h>
#include <cstring>
#include "../../logger.h"

#if NODEJS_API
	#include <nan.h>
	#include <include/NanEventCallback.h>
#endif

namespace tc::ft {
    namespace error {
        enum value : int8_t {
            success = 0,
            custom = 1,
            custom_recoverable = 2,
            would_block = 3,
            out_of_space = 4
        };
    }
    class TransferObject {
        public:
            explicit TransferObject() = default;

            [[nodiscard]] virtual std::string name() const = 0;
            virtual bool initialize(std::string& /* error */) = 0;
            virtual void finalize(bool /* aborted */) = 0;
    };

    class TransferSource : public TransferObject {
        public:
            [[nodiscard]] virtual uint64_t byte_length() const = 0;
            [[nodiscard]] virtual uint64_t stream_index() const = 0;

            virtual error::value read_bytes(std::string& /* error */, uint8_t* /* buffer */, uint64_t& /* max length/result length */) = 0;
        private:
    };

    class TransferTarget : public TransferObject {
        public:
            TransferTarget() = default;

            [[nodiscard]] virtual uint64_t expected_length() const = 0;
            [[nodiscard]] virtual uint64_t stream_index() const = 0;

            virtual error::value write_bytes(std::string& /* error */, uint8_t* /* buffer */, uint64_t /* max length */) = 0;
    };

    struct TransferOptions {
        std::string remote_address;
        uint16_t remote_port = 0;
        std::string transfer_key{};
        uint32_t client_transfer_id = 0;
        uint32_t server_transfer_id = 0;
    };

    class FileTransferManager;
    class Transfer {
            friend class FileTransferManager;
        public:
            struct state {
                enum value {
                    UNINITIALIZED,
                    CONNECTING,
                    CONNECTED,
                    DISCONNECTING
                };
            };
            typedef std::function<void()> callback_start_t;
            typedef std::function<void(bool /* aborted */)> callback_finished_t;
            typedef std::function<void(const std::string& /* error */)> callback_failed_t;
            typedef std::function<void(uint64_t /* current index */, uint64_t /* max index */)> callback_process_t;

            explicit Transfer(FileTransferManager* handle, std::shared_ptr<TransferObject> transfer_object, std::unique_ptr<TransferOptions> options) :
                        _transfer_object(std::move(transfer_object)),
                        _handle(handle),
                        _options(std::move(options)) {
                log_allocate("Transfer", this);
            }
            ~Transfer();

            bool initialize(std::string& /* error */);
            void finalize(bool /* blocking */, bool /* aborted */);

            bool connect();
            bool connected() { return this->_state > state::UNINITIALIZED; }

            FileTransferManager* handle() { return this->_handle; }
            std::shared_ptr<TransferObject> transfer_object() { return this->_transfer_object; }
            const TransferOptions& options() { return *this->_options; }

            callback_start_t callback_start{nullptr};
            callback_finished_t callback_finished{nullptr};
            callback_failed_t callback_failed{nullptr};
            callback_process_t callback_process{nullptr};
        private:
            static void _callback_read(evutil_socket_t, short, void*);
            static void _callback_write(evutil_socket_t, short, void*);

            sockaddr_storage remote_address{};
            FileTransferManager* _handle;
            std::unique_ptr<TransferOptions> _options;
            state::value _state = state::UNINITIALIZED;
            std::shared_ptr<TransferObject> _transfer_object;

            std::mutex event_lock;
            event_base* event_io = nullptr; /* gets assigned by the manager */
            ::event* event_read = nullptr;
            ::event* event_write = nullptr;

            std::chrono::system_clock::time_point last_source_read;
            std::chrono::system_clock::time_point last_target_write;
            std::mutex queue_lock;
            std::deque<pipes::buffer> write_queue;
            void _write_message(const pipes::buffer_view& /* buffer */);
            int _socket = 0;

            timeval alive_check_timeout{1, 0};
            timeval write_timeout{1, 0};

            /*
             * Upload mode:
             *  Write the buffers left in write_queue, and if the queue length is less then 12 create new buffers.
             *  This event will as well be triggered every second as timeout, to create new buffers if needed
             */
            void callback_write(short /* flags */);
            void callback_read(short /* flags */);

            /* called within the write/read callback */
            void handle_disconnect(bool /* write disconnect */);
            void handle_connected();

            void call_callback_connected();
            void call_callback_failed(const std::string& /* reason */);
            void call_callback_finished(bool /* aborted */);
            void call_callback_process(size_t /* current */, size_t /* max */);

            std::chrono::system_clock::time_point last_process_call;
    };

    class FileTransferManager {
        public:
            FileTransferManager();
            ~FileTransferManager();

            void initialize();
            void finalize();

            std::shared_ptr<Transfer> register_transfer(std::string& error, const std::shared_ptr<TransferObject>& /* object */, std::unique_ptr<TransferOptions> /* options */);
            std::deque<std::shared_ptr<Transfer>> running_transfers() {
                std::lock_guard lock(this->_transfer_lock);
                return this->_running_transfers;
            }
            void drop_transfer(const std::shared_ptr<Transfer>& /* transfer */);
            void remove_transfer(Transfer*); /* internal use */
        private:
            bool event_execute = false;
            bool event_io_canceled = false;
            std::thread event_io_thread;
            event_base* event_io = nullptr;
            ::event* event_cleanup = nullptr;

            std::mutex _transfer_lock;
            std::deque<std::shared_ptr<Transfer>> _running_transfers;

            void _execute_event_loop();
    };

#ifdef NODEJS_API
    class JSTransfer : public Nan::ObjectWrap {
        public:
            static NAN_MODULE_INIT(Init);
            static NAN_METHOD(NewInstance);

            static inline Nan::Persistent<v8::Function> & constructor() {
                static Nan::Persistent<v8::Function> my_constructor;
                return my_constructor;
            }

            explicit JSTransfer(std::shared_ptr<Transfer> transfer);
            ~JSTransfer() override;

            NAN_METHOD(start);
            NAN_METHOD(abort);

            static NAN_METHOD(destory_transfer);
        private:
            static NAN_METHOD(_start);
            static NAN_METHOD(_abort);

            std::shared_ptr<Transfer> _transfer;

            Nan::callback_t<bool> call_finished;
            Nan::callback_t<> call_start;
            Nan::callback_t<uint64_t, uint64_t> call_progress;
            Nan::callback_t<std::string> call_failed;

            void callback_finished(bool);
            void callback_start();
            void callback_progress(uint64_t, uint64_t);
            void callback_failed(std::string);

            bool _self_ref = false;
    };
#endif
}
extern tc::ft::FileTransferManager* transfer_manager;