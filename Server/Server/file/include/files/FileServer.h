#pragma once

#include <string>
#include <chrono>
#include <utility>
#include <variant>
#include <deque>
#include <functional>
#include <atomic>
#include <condition_variable>

#include "./ExecuteResponse.h"

#define TRANSFER_KEY_LENGTH (32)
#define TRANSFER_MEDIA_BYTES_LENGTH (32)

namespace ts::server::file {
    class VirtualFileServer;

    typedef uint64_t ChannelId;
    typedef uint64_t ServerId;
    typedef uint64_t ClientId;

    namespace filesystem {
        template <typename ErrorCodes>
        struct DetailedError {
            ErrorCodes error_type{ErrorCodes::UNKNOWN};
            std::string error_message{};

            DetailedError(ErrorCodes type, std::string extraMessage) : error_type{type}, error_message{std::move(extraMessage)} {}
        };

        enum struct DirectoryQueryErrorType {
            UNKNOWN,
            PATH_EXCEEDS_ROOT_PATH,
            PATH_IS_A_FILE,
            PATH_DOES_NOT_EXISTS,
            FAILED_TO_LIST_FILES
        };
        constexpr std::array<std::string_view, 5> directory_query_error_messages = {
            "unknown error",
            "path exceeds base path",
            "path is a file",
            "path does not exists",
            "failed to list files"
        };

        typedef DetailedError<DirectoryQueryErrorType> DirectoryQueryError;

        struct DirectoryEntry {
            enum Type {
                UNKNOWN,
                DIRECTORY,
                FILE
            };

            Type type{Type::UNKNOWN};
            std::string name{};
            std::chrono::system_clock::time_point modified_at{};

            size_t size{0}; /* file only */
            bool empty{false}; /* directory only */
        };

        enum struct DirectoryModifyErrorType {
            UNKNOWN,
            PATH_EXCEEDS_ROOT_PATH,
            PATH_ALREADY_EXISTS,
            FAILED_TO_CREATE_DIRECTORIES
        };
        typedef DetailedError<DirectoryModifyErrorType> DirectoryModifyError;

        enum struct FileModifyErrorType {
            UNKNOWN,
            PATH_EXCEEDS_ROOT_PATH,
            TARGET_PATH_EXCEEDS_ROOT_PATH,
            PATH_DOES_NOT_EXISTS,
            TARGET_PATH_ALREADY_EXISTS,
            FAILED_TO_DELETE_FILES,
            FAILED_TO_RENAME_FILE,
            FAILED_TO_CREATE_DIRECTORIES,

            SOME_FILES_ARE_LOCKED
        };
        typedef DetailedError<FileModifyErrorType> FileModifyError;

        enum struct FileDeleteErrorType {
            UNKNOWN,
        };
        typedef DetailedError<FileDeleteErrorType> FileDeleteError;

        struct FileDeleteResponse {
            enum struct StatusType {
                SUCCESS,

                PATH_EXCEEDS_ROOT_PATH,
                PATH_DOES_NOT_EXISTS,
                FAILED_TO_DELETE_FILES,
                SOME_FILES_ARE_LOCKED
            };

            struct DeleteResult {
                StatusType status{StatusType::SUCCESS};
                std::string error_detail{};

                DeleteResult(StatusType status, std::string errorDetail) : status{status},
                                                                                  error_detail{std::move(errorDetail)} {}
                DeleteResult() = default;
            };

            std::vector<DeleteResult> delete_results{};
        };

        enum struct ServerCommandErrorType {
            UNKNOWN,
            FAILED_TO_CREATE_DIRECTORIES,
            FAILED_TO_DELETE_DIRECTORIES
        };
        typedef DetailedError<ServerCommandErrorType> ServerCommandError;

        struct FileInfoResponse {
            enum struct StatusType {
                SUCCESS,

                PATH_EXCEEDS_ROOT_PATH,
                PATH_DOES_NOT_EXISTS,

                FAILED_TO_QUERY_INFO,
                UNKNOWN_FILE_TYPE
            };

            struct FileInfo {
                StatusType status{StatusType::SUCCESS};
                std::string error_detail{};

                DirectoryEntry info{};

                FileInfo(StatusType status, std::string errorDetail, DirectoryEntry info) : status{status},
                                                                                  error_detail{std::move(errorDetail)}, info{std::move(info)} {}
                FileInfo() = default;
            };

            std::vector<FileInfo> file_info{};
        };

        enum struct FileInfoErrorType {
            UNKNOWN,
        };
        typedef DetailedError<FileInfoErrorType> FileInfoError;

        class AbstractProvider {
            public:
                typedef ExecuteResponse<DirectoryQueryError, std::deque<DirectoryEntry>> directory_query_response_t;

                /* server */
                [[nodiscard]] virtual std::shared_ptr<ExecuteResponse<ServerCommandError>> initialize_server(const std::shared_ptr<VirtualFileServer> &/* server */) = 0;

                /* channels */
                [[nodiscard]] virtual std::shared_ptr<ExecuteResponse<FileInfoError, FileInfoResponse>> query_channel_info(const std::shared_ptr<VirtualFileServer> &/* server */, const std::vector<std::tuple<ChannelId, std::string>>& /* files */) = 0;
                [[nodiscard]] virtual std::shared_ptr<directory_query_response_t> query_channel_directory(const std::shared_ptr<VirtualFileServer> &/* server */, ChannelId /* channel */, const std::string& /* path */) = 0;
                [[nodiscard]] virtual std::shared_ptr<ExecuteResponse<DirectoryModifyError>> create_channel_directory(const std::shared_ptr<VirtualFileServer> &/* server */, ChannelId /* channel */, const std::string& /* path */) = 0;
                [[nodiscard]] virtual std::shared_ptr<ExecuteResponse<FileDeleteError, FileDeleteResponse>> delete_channel_files(const std::shared_ptr<VirtualFileServer> &/* server */, ChannelId /* channel */, const std::vector<std::string>& /* paths */) = 0;
                [[nodiscard]] virtual std::shared_ptr<ExecuteResponse<FileModifyError>> rename_channel_file(const std::shared_ptr<VirtualFileServer> &/* server */, ChannelId /* channel */, const std::string& /* path */, ChannelId /* target channel */, const std::string& /* target */) = 0;

                /* icons */
                [[nodiscard]] virtual std::shared_ptr<ExecuteResponse<FileInfoError, FileInfoResponse>> query_icon_info(const std::shared_ptr<VirtualFileServer> &/* server */, const std::vector<std::string>& /* names */) = 0;
                [[nodiscard]] virtual std::shared_ptr<directory_query_response_t> query_icon_directory(const std::shared_ptr<VirtualFileServer> &/* server */) = 0;
                [[nodiscard]] virtual std::shared_ptr<ExecuteResponse<FileDeleteError, FileDeleteResponse>> delete_icons(const std::shared_ptr<VirtualFileServer> &/* server */, const std::vector<std::string>& /* names */) = 0;

                /* avatars */
                [[nodiscard]] virtual std::shared_ptr<ExecuteResponse<FileInfoError, FileInfoResponse>> query_avatar_info(const std::shared_ptr<VirtualFileServer> &/* server */, const std::vector<std::string>& /* names */) = 0;
                [[nodiscard]] virtual std::shared_ptr<directory_query_response_t> query_avatar_directory(const std::shared_ptr<VirtualFileServer> &/* server */) = 0;
                [[nodiscard]] virtual std::shared_ptr<ExecuteResponse<FileDeleteError, FileDeleteResponse>> delete_avatars(const std::shared_ptr<VirtualFileServer> &/* server */, const std::vector<std::string>& /* names */) = 0;
            private:
        };
    }

    namespace transfer {
        typedef uint16_t transfer_id;

        struct Transfer {
            transfer_id server_transfer_id{0};
            transfer_id client_transfer_id{0};

            std::shared_ptr<VirtualFileServer> server{nullptr};
            ChannelId channel_id{0};

            ClientId client_id{0};
            std::string client_unique_id{};

            std::string transfer_key{};
            std::chrono::system_clock::time_point initialized_timestamp{};
            enum Direction {
                DIRECTION_UNKNOWN,
                DIRECTION_UPLOAD,
                DIRECTION_DOWNLOAD
            } direction{DIRECTION_UNKNOWN};

            struct Address {
                std::string hostname{};
                uint16_t port{0};
            };
            std::vector<Address> server_addresses{};

            enum TargetType {
                TARGET_TYPE_UNKNOWN,
                TARGET_TYPE_CHANNEL_FILE,
                TARGET_TYPE_ICON,
                TARGET_TYPE_AVATAR
            } target_type{TARGET_TYPE_UNKNOWN};
            std::string target_file_path{};
            std::string absolute_file_path{};

            std::string relative_file_path{};
            std::string file_name{};

            int64_t max_bandwidth{-1};
            size_t expected_file_size{0}; /* incl. the offset! */
            size_t file_offset{0};
            bool override_exiting{false};
        };

        struct TransferStatistics {
            uint64_t network_bytes_send{0};
            uint64_t network_bytes_received{0};

            uint64_t delta_network_bytes_send{0};
            uint64_t delta_network_bytes_received{0};

            uint64_t file_bytes_transferred{0};
            uint64_t delta_file_bytes_transferred{0};

            size_t file_start_offset{0};
            size_t file_current_offset{0};
            size_t file_total_size{0};

            double average_speed{0};
            double current_speed{0};
        };

        struct TransferInitError {
            enum Type {
                UNKNOWN,

                INVALID_FILE_TYPE,
                FILE_DOES_NOT_EXISTS,
                FILE_IS_NOT_A_FILE,

                CLIENT_TOO_MANY_TRANSFERS,
                SERVER_TOO_MANY_TRANSFERS,

                SERVER_QUOTA_EXCEEDED,
                CLIENT_QUOTA_EXCEEDED,

                IO_ERROR
            } error_type{UNKNOWN};
            std::string error_message{};

            TransferInitError(Type errorType, std::string errorMessage) : error_type{errorType},
                                                                                 error_message{std::move(errorMessage)} {}
        };

        struct TransferActionError {
            enum Type {
                UNKNOWN,

                UNKNOWN_TRANSFER
            } error_type{UNKNOWN};
            std::string error_message{};
        };

        struct TransferError {
            enum Type {
                UNKNOWN,

                TRANSFER_TIMEOUT,

                DISK_IO_ERROR,
                DISK_TIMEOUT,
                DISK_INITIALIZE_ERROR,

                NETWORK_IO_ERROR,

                UNEXPECTED_CLIENT_DISCONNECT,
                UNEXPECTED_DISK_EOF,

                USER_REQUEST
            } error_type{UNKNOWN};
            std::string error_message{};
        };

        struct ActiveFileTransfer {
            transfer_id client_transfer_id{0};
            transfer_id server_transfer_id{0};

            Transfer::Direction direction{Transfer::DIRECTION_UNKNOWN};

            ClientId client_id{};
            std::string client_unique_id{};

            std::string file_path{};
            std::string file_name{};

            size_t expected_size{};
            size_t size_done{};

            enum Status {
                NOT_STARTED,
                RUNNING,
                INACTIVE /* (not used yet) */
            } status{Status::NOT_STARTED};

            std::chrono::milliseconds runtime{};

            double average_speed{0};
            double current_speed{0};
        };

        enum struct TransferListError {
            UNKNOWN
        };

        class AbstractProvider {
            public:
                struct TransferInfo {
                    std::string file_path{};
                    std::string client_unique_id{};
                    ClientId client_id{};

                    bool override_exiting{false}; /* only for upload valid */

                    size_t file_offset{0};
                    size_t expected_file_size{0};

                    int64_t max_bandwidth{-1};
                    int64_t max_concurrent_transfers{-1};

                    /* only used for upload, for download the quotas could be checked before */
                    size_t download_server_quota_limit{(size_t) -1};
                    size_t download_client_quota_limit{(size_t) -1};
                };

                virtual std::shared_ptr<ExecuteResponse<TransferInitError, std::shared_ptr<Transfer>>>
                initialize_channel_transfer(Transfer::Direction /* direction */, const std::shared_ptr<VirtualFileServer>& /* server */, ChannelId /* channel */, const TransferInfo& /* info */) = 0;

                virtual std::shared_ptr<ExecuteResponse<TransferInitError, std::shared_ptr<Transfer>>>
                initialize_icon_transfer(Transfer::Direction /* direction */, const std::shared_ptr<VirtualFileServer>& /* server */, const TransferInfo& /* info */) = 0;

                virtual std::shared_ptr<ExecuteResponse<TransferInitError, std::shared_ptr<Transfer>>>
                initialize_avatar_transfer(Transfer::Direction /* direction */, const std::shared_ptr<VirtualFileServer>& /* server */, const TransferInfo& /* info */) = 0;

                virtual std::shared_ptr<ExecuteResponse<TransferListError, std::vector<ActiveFileTransfer>>> list_transfer() = 0;

                virtual std::shared_ptr<ExecuteResponse<TransferActionError>> stop_transfer(const std::shared_ptr<VirtualFileServer>& /* server */, transfer_id /* id */, bool /* flush */) = 0;

                std::function<void(const std::shared_ptr<Transfer>&)> callback_transfer_registered{}; /* transfer has been registered */
                std::function<void(const std::shared_ptr<Transfer>&)> callback_transfer_started{}; /* transfer has been started */
                std::function<void(const std::shared_ptr<Transfer>&)> callback_transfer_finished{}; /* transfer has been finished */
                std::function<void(const std::shared_ptr<Transfer>&, const TransferStatistics&)> callback_transfer_statistics{};
                std::function<void(const std::shared_ptr<Transfer>&, const transfer::TransferStatistics&, const TransferError&)> callback_transfer_aborted{}; /* an error happened while transferring the data  */
        };
    }

    class VirtualFileServer {
        public:
            explicit VirtualFileServer(ServerId server_id, std::string unique_id) : server_id_{server_id}, unique_id_{std::move(unique_id)} {}

            [[nodiscard]] inline auto unique_id() const -> const std::string& { return this->unique_id_; }
            [[nodiscard]] inline auto server_id() const -> ServerId { return this->server_id_; }

            [[nodiscard]] inline auto max_networking_upload_bandwidth() const -> int64_t { return this->max_networking_upload_bandwidth_; }
            virtual void max_networking_upload_bandwidth(int64_t value) {
                this->max_networking_upload_bandwidth_ = value;
            }

            [[nodiscard]] inline auto max_networking_download_bandwidth() const -> int64_t { return this->max_networking_download_bandwidth_; }
            virtual void max_networking_download_bandwidth(int64_t value) {
                this->max_networking_download_bandwidth_ = value;
            }

            [[nodiscard]] inline auto generate_transfer_id() {
                return ++this->current_transfer_id;
            }
        private:
            ServerId server_id_;
            std::string unique_id_;

            int64_t max_networking_upload_bandwidth_{-1};
            int64_t max_networking_download_bandwidth_{-1};

            std::atomic<transfer::transfer_id> current_transfer_id{0};
    };

    class AbstractFileServer {
        public:
            [[nodiscard]] virtual std::string file_base_path() const = 0;
            [[nodiscard]] virtual filesystem::AbstractProvider& file_system() = 0;
            [[nodiscard]] virtual transfer::AbstractProvider& file_transfer() = 0;

            [[nodiscard]] inline auto virtual_servers() const -> std::deque<std::shared_ptr<VirtualFileServer>> {
                std::lock_guard slock{this->servers_mutex};
                return this->servers_;
            }

            [[nodiscard]] inline auto find_virtual_server(ServerId server_id) const -> std::shared_ptr<VirtualFileServer> {
                std::lock_guard slock{this->servers_mutex};
                auto it = std::find_if(this->servers_.begin(), this->servers_.end(), [&](const std::shared_ptr<VirtualFileServer>& server) {
                    return server->server_id() == server_id;
                });
                return it == this->servers_.end() ? nullptr : *it;
            }

            virtual std::shared_ptr<VirtualFileServer> register_server(ServerId /* server id */) = 0;
            virtual void unregister_server(ServerId /* server id */, bool /* delete tiles */) = 0;
        protected:
            mutable std::mutex servers_mutex{};
            std::deque<std::shared_ptr<VirtualFileServer>> servers_{};
    };

    extern bool initialize(std::string& /* error */, const std::string& /* host names */, uint16_t /* port */);
    extern void finalize();

    extern std::shared_ptr<AbstractFileServer> server();
}