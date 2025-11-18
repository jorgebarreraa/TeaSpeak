//
// Created by WolverinDEV on 16/09/2020.
//

#pragma once

#pragma once

#include <files/FileServer.h>
#include <deque>
#include <utility>
#include <thread>
#include <shared_mutex>
#include <sys/socket.h>
#include <pipes/ws.h>
#include <pipes/ssl.h>
#include <misc/net.h>
#include <misc/spin_mutex.h>
#include <random>
#include <misc/memtracker.h>
#include "./NetTools.h"
#include "LocalFileSystem.h"

#define TRANSFER_MAX_CACHED_BYTES (1024 * 1024 * 1) // Buffer up to 1mb

namespace ts::server::file::transfer {
    class LocalFileTransfer;

    struct Buffer {
        Buffer* next{nullptr};

        std::atomic_uint32_t ref_count{0};
        uint32_t capacity{0};
        uint32_t length{0};
        uint32_t offset{0};

        char data[1]{};
    };

    [[nodiscard]] extern Buffer* allocate_buffer(size_t);
    [[nodiscard]] extern Buffer* ref_buffer(Buffer*);
    extern void deref_buffer(Buffer*);

    /* all variables are locked via the state_mutex */
    struct FileClient : std::enable_shared_from_this<FileClient> {
        LocalFileTransfer* handle;
        std::shared_ptr<Transfer> transfer{nullptr};

        std::shared_mutex state_mutex{};
        enum {
            STATE_AWAITING_KEY, /* includes SSL/HTTP init */
            STATE_TRANSFERRING,
            STATE_FLUSHING,
            STATE_DISCONNECTED
        } state{STATE_AWAITING_KEY};

        bool finished_signal_send{false};

        enum NetworkingProtocol {
            PROTOCOL_UNKNOWN,
            PROTOCOL_HTTPS,
            PROTOCOL_TS_V1
        };

        enum HTTPUploadState {
            HTTP_AWAITING_HEADER,
            HTTP_STATE_AWAITING_BOUNDARY,
            HTTP_STATE_AWAITING_BOUNDARY_END,
            HTTP_STATE_UPLOADING,
            HTTP_STATE_DOWNLOADING
        };

        struct {
            bool file_locked{false};
            int file_descriptor{0};

            bool currently_processing{false};
            FileClient* next_client{nullptr};

            bool query_media_bytes{false};
            uint8_t media_bytes[TRANSFER_MEDIA_BYTES_LENGTH]{0};
            uint8_t media_bytes_length{0};
        } file{};

        struct {
            size_t provided_bytes{0};
            char key[TRANSFER_KEY_LENGTH]{0};
        } transfer_key{};

        struct {
            std::mutex mutex{};
            size_t bytes{0};

            bool buffering_stopped{false};
            bool write_disconnected{false};

            Buffer* buffer_head{nullptr};
            Buffer** buffer_tail{&buffer_head};
        } network_buffer{};

        struct {
            std::mutex mutex{};
            size_t bytes{0};

            bool buffering_stopped{false};
            bool write_disconnected{false};

            Buffer* buffer_head{nullptr};
            Buffer** buffer_tail{&buffer_head};
        } disk_buffer{};

        struct {
            sockaddr_storage address{};
            int file_descriptor{0};

            NetworkingProtocol protocol{PROTOCOL_UNKNOWN};

            struct event* event_read{nullptr};
            struct event* event_write{nullptr};
            struct event* event_throttle{nullptr};

            bool add_event_write{false}, add_event_read{false};

            std::chrono::system_clock::time_point disconnect_timeout{};

            networking::NetworkThrottle client_throttle{};
            /* the right side is the server throttle */
            networking::DualNetworkThrottle throttle{&client_throttle, &networking::NetworkThrottle::kNoThrottle};

            pipes::SSL pipe_ssl{};
            bool pipe_ssl_init{false};
            std::unique_ptr<Buffer, decltype(deref_buffer)*> http_header_buffer{nullptr, deref_buffer};
            HTTPUploadState http_state{HTTPUploadState::HTTP_AWAITING_HEADER};

            std::string http_boundary{};

            /* Only read the transfer key length at the beginning. We than have the actual limit which will be set via throttle */
            size_t max_read_buffer_size{TRANSFER_KEY_LENGTH};
        } networking{};

        struct {
            networking::TransferStatistics network_send{};
            networking::TransferStatistics network_received{};

            networking::TransferStatistics file_transferred{};

            networking::TransferStatistics disk_bytes_read{};
            networking::TransferStatistics disk_bytes_write{};
        } statistics{};

        struct {
            std::chrono::system_clock::time_point last_write{};
            std::chrono::system_clock::time_point last_read{};

            std::chrono::system_clock::time_point connected{};
            std::chrono::system_clock::time_point key_received{};
            std::chrono::system_clock::time_point disconnecting{};
        } timings;

        explicit FileClient(LocalFileTransfer* handle) : handle{handle} { memtrack::allocated<FileClient>(this); }
        ~FileClient();

        void add_network_write_event(bool /* ignore bandwidth limits */);
        void add_network_write_event_nolock(bool /* ignore bandwidth limits */);

        /* will check if we've enough space in out read buffer again */
        void add_network_read_event(bool /* ignore bandwidth limits */);

        bool send_file_bytes(const void* /* buffer */, size_t /* length */);
        bool enqueue_network_buffer_bytes(const void* /* buffer */, size_t /* length */);
        bool enqueue_disk_buffer_bytes(const void* /* buffer */, size_t /* length */);

        /* these function clear the buffers and set the write disconnected flags to true so no new buffers will be enqueued */
        size_t flush_network_buffer();
        void flush_disk_buffer();

        [[nodiscard]] bool buffers_flushed();
        [[nodiscard]] inline std::string log_prefix() const { return "[" + net::to_string(this->networking.address) + "]"; }
    };

    enum struct DiskIOStartResult {
        SUCCESS,
        OUT_OF_MEMORY
    };

    enum struct NetworkingStartResult {
        SUCCESS,
        OUT_OF_MEMORY
    };

    enum struct NetworkingBindResult {
        SUCCESS,

        BINDING_ALREADY_EXISTS,
        NETWORKING_NOT_INITIALIZED,
        FAILED_TO_ALLOCATE_SOCKET, /* errno is set */
        FAILED_TO_BIND,
        FAILED_TO_LISTEN,

        OUT_OF_MEMORY,
    };

    enum struct NetworkingUnbindResult {
        SUCCESS,
        UNKNOWN_BINDING
    };

    enum struct ClientWorkerStartResult {
        SUCCESS
    };

    enum struct NetworkInitializeResult {
        SUCCESS,
        OUT_OF_MEMORY
    };

    enum struct FileInitializeResult {
        SUCCESS,

        INVALID_TRANSFER_DIRECTION,
        OUT_OF_MEMORY,

        PROCESS_FILE_LIMIT_REACHED,
        SYSTEM_FILE_LIMIT_REACHED,

        FILE_IS_BUSY,
        FILE_DOES_NOT_EXISTS,
        FILE_SYSTEM_ERROR,
        FILE_IS_A_DIRECTORY,

        FILE_TOO_LARGE,
        DISK_IS_READ_ONLY,

        FILE_SEEK_FAILED,
        FILE_SIZE_MISMATCH,

        FILE_IS_NOT_ACCESSIBLE,

        FAILED_TO_READ_MEDIA_BYTES,
        COUNT_NOT_CREATE_DIRECTORIES,

        MAX
    };

    constexpr static std::array<std::string_view, (size_t) FileInitializeResult::MAX> kFileInitializeResultMessages{
            /* SUCCESS */                       "success",

            /* INVALID_TRANSFER_DIRECTION */    "invalid file transfer direction",
            /* OUT_OF_MEMORY */                 "out of memory",

            /* PROCESS_FILE_LIMIT_REACHED */    "process file limit reached",
            /* SYSTEM_FILE_LIMIT_REACHED */     "system file limit reached",

            /* FILE_IS_BUSY */                  "target file is busy",
            /* FILE_DOES_NOT_EXISTS */          "target file does not exists",
            /* FILE_SYSTEM_ERROR */             "internal file system error",
            /* FILE_IS_A_DIRECTORY */           "target file is a directory",

            /* FILE_TOO_LARGE */                "file is too large",
            /* DISK_IS_READ_ONLY */             "disk is in read only mode",

            /* FILE_SEEK_FAILED */              "failed to seek to target file offset",
            /* FILE_SIZE_MISMATCH */            "file size miss match",
            /* FILE_IS_NOT_ACCESSIBLE */        "file is not accessible",
            /* FAILED_TO_READ_MEDIA_BYTES */    "failed to read file media bytes",
            /* COUNT_NOT_CREATE_DIRECTORIES */  "could not create required directories"
    };

    enum struct TransferKeyApplyResult {
        SUCCESS,
        FILE_ERROR,
        UNKNOWN_KEY,

        INTERNAL_ERROR
    };

    enum struct TransferUploadRawResult {
        MORE_DATA_TO_RECEIVE,
        FINISH,
        FINISH_OVERFLOW,

        /* UNKNOWN ERROR */
    };

    enum struct TransferUploadHTTPResult {
        MORE_DATA_TO_RECEIVE,
        FINISH,

        BOUNDARY_MISSING,
        BOUNDARY_TOKEN_INVALID,
        BOUNDARY_INVALID,

        MISSING_CONTENT_TYPE,
        INVALID_CONTENT_TYPE
        /* UNKNOWN ERROR */
    };

    struct NetworkBinding {
        std::string hostname{};
        sockaddr_storage address{};
    };

    struct ActiveNetworkBinding : std::enable_shared_from_this<ActiveNetworkBinding> {
        std::string hostname{};
        sockaddr_storage address{};

        int file_descriptor{-1};
        struct event* accept_event{nullptr};

        LocalFileTransfer* handle{nullptr};
    };

    class LocalFileTransfer : public AbstractProvider {
        public:
            explicit LocalFileTransfer(filesystem::LocalFileSystem*);
            ~LocalFileTransfer();

            [[nodiscard]] bool start();
            void stop();

            [[nodiscard]] NetworkingBindResult add_network_binding(const NetworkBinding& /* binding */);
            [[nodiscard]] std::vector<NetworkBinding> active_network_bindings();
            [[nodiscard]] NetworkingUnbindResult remove_network_binding(const NetworkBinding& /* binding */);

            std::shared_ptr<ExecuteResponse<TransferInitError, std::shared_ptr<Transfer>>>
            initialize_channel_transfer(Transfer::Direction direction, const std::shared_ptr<VirtualFileServer>& server, ChannelId channelId,
                                        const TransferInfo &info) override;

            std::shared_ptr<ExecuteResponse<TransferInitError, std::shared_ptr<Transfer>>>
            initialize_icon_transfer(Transfer::Direction direction, const std::shared_ptr<VirtualFileServer>& server, const TransferInfo &info) override;

            std::shared_ptr<ExecuteResponse<TransferInitError, std::shared_ptr<Transfer>>>
            initialize_avatar_transfer(Transfer::Direction direction, const std::shared_ptr<VirtualFileServer>& server, const TransferInfo &info) override;

            std::shared_ptr<ExecuteResponse<TransferListError, std::vector<ActiveFileTransfer>>> list_transfer() override;

            std::shared_ptr<ExecuteResponse<TransferActionError>> stop_transfer(const std::shared_ptr<VirtualFileServer>& /* server */, transfer_id id, bool) override;
        private:
            enum struct DiskIOLoopState {
                STOPPED,
                RUNNING,

                STOPPING,
                FORCE_STOPPING
            };
            filesystem::LocalFileSystem* file_system_;

            size_t max_concurrent_transfers{1024};
            std::mt19937 transfer_random_token_generator{std::random_device{}()};

            std::mutex result_notify_mutex{};
            std::condition_variable result_notify_cv{};

            std::mutex transfers_mutex{};
            std::mutex transfer_create_mutex{};
            std::deque<std::shared_ptr<FileClient>> transfers_{};
            std::deque<std::shared_ptr<Transfer>> pending_transfers{};

            enum ServerState {
                STOPPED,
                RUNNING
            } state{ServerState::STOPPED};

            struct {
                bool active{false};

                std::thread dispatch_thread{};
                std::mutex mutex{};
                std::condition_variable notify_cv{};
            } disconnect;

            struct {
                std::mutex mutex;

                bool active{false};
                std::thread dispatch_thread{};
                struct event_base* event_base{nullptr};

                std::deque<std::shared_ptr<ActiveNetworkBinding>> bindings{};
            } network{};

            struct {
                DiskIOLoopState state{DiskIOLoopState::STOPPED};
                std::thread dispatch_thread{};
                std::mutex queue_lock{};
                std::condition_variable notify_work_awaiting{};
                std::condition_variable notify_client_processed{};

                FileClient* queue_head{nullptr};
                FileClient** queue_tail{&queue_head};
            } disk_io{};

            template <typename error_t, typename result_t = EmptyExecuteResponse>
            std::shared_ptr<ExecuteResponse<error_t, result_t>> create_execute_response() {
                return std::make_shared<ExecuteResponse<error_t, result_t>>(this->result_notify_mutex, this->result_notify_cv);
            }

            std::shared_ptr<ExecuteResponse<TransferInitError, std::shared_ptr<Transfer>>>
            initialize_transfer(Transfer::Direction, const std::shared_ptr<VirtualFileServer> &, ChannelId, Transfer::TargetType, const TransferInfo &info);

            [[nodiscard]] NetworkingStartResult start_networking();
            [[nodiscard]] DiskIOStartResult start_disk_io();
            [[nodiscard]] ClientWorkerStartResult start_client_worker();

            void shutdown_networking();
            void shutdown_disk_io();
            void shutdown_client_worker();

            void disconnect_client(const std::shared_ptr<FileClient>& /* client */, std::unique_lock<std::shared_mutex>& /* state lock */, bool /* flush network */);

            [[nodiscard]] NetworkInitializeResult initialize_networking(const std::shared_ptr<FileClient>& /* client */, int /* file descriptor */);
            /* might block 'till all IO operations have been succeeded */
            void finalize_networking(const std::shared_ptr<FileClient>& /* client */, std::unique_lock<std::shared_mutex>& /* state lock */);

            [[nodiscard]] FileInitializeResult initialize_file_io(const std::shared_ptr<FileClient>& /* client */);
            void finalize_file_io(const std::shared_ptr<FileClient>& /* client */, std::unique_lock<std::shared_mutex>& /* state lock */);

            [[nodiscard]] bool initialize_client_ssl(const std::shared_ptr<FileClient>& /* client */);
            void finalize_client_ssl(const std::shared_ptr<FileClient>& /* client */);

            void enqueue_disk_io(const std::shared_ptr<FileClient>& /* client */);
            void execute_disk_io(const std::shared_ptr<FileClient>& /* client */);

            void test_disconnecting_state(const std::shared_ptr<FileClient>& /* client */);

            [[nodiscard]] TransferUploadRawResult handle_transfer_upload_raw(const std::shared_ptr<FileClient>& /* client */, const char * /* buffer */, size_t /* length */, size_t* /* bytes written */);
            [[nodiscard]] TransferUploadHTTPResult handle_transfer_upload_http(const std::shared_ptr<FileClient>& /* client */, const char * /* buffer */, size_t /* length */);

            void send_http_response(const std::shared_ptr<FileClient>& /* client */, http::HttpResponse& /* response */);

            static void callback_transfer_network_write(int, short, void*);
            static void callback_transfer_network_read(int, short, void*);
            static void callback_transfer_network_throttle(int, short, void*);
            static void callback_transfer_network_accept(int, short, void*);

            static void dispatch_loop_client_worker(void*);
            static void dispatch_loop_network(void*);
            static void dispatch_loop_disk_io(void*);

            void report_transfer_statistics(const std::shared_ptr<FileClient>& /* client */);
            [[nodiscard]] TransferStatistics generate_transfer_statistics_report(const std::shared_ptr<FileClient>& /* client */);
            void invoke_aborted_callback(const std::shared_ptr<FileClient>& /* client */, const TransferError& /* error */);
            void invoke_aborted_callback(const std::shared_ptr<Transfer>& /* pending transfer */, const TransferError& /* error */);

            size_t handle_transfer_read(const std::shared_ptr<FileClient>& /* client */, const char* /* buffer */, size_t /* bytes */);
            size_t handle_transfer_read_raw(const std::shared_ptr<FileClient>& /* client */, const char* /* buffer */, size_t /* bytes */);
            [[nodiscard]] TransferKeyApplyResult handle_transfer_key_provided(const std::shared_ptr<FileClient>& /* client */, std::string& /* error */);
    };
}