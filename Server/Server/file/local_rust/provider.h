//
// Created by WolverinDEV on 20/04/2021.
//

#pragma once

#include <files/FileServer.h>

namespace ts::server::file {
    namespace filesystem {
        class RustFileSystem : public filesystem::AbstractProvider {
                using FileModifyError = filesystem::FileModifyError;
                using DirectoryModifyError = filesystem::DirectoryModifyError;
            public:
                virtual ~RustFileSystem();

                std::shared_ptr<ExecuteResponse<ServerCommandError>> initialize_server(const std::shared_ptr<VirtualFileServer> & /* server */) override;

                std::shared_ptr<ExecuteResponse<FileInfoError, FileInfoResponse>>
                query_channel_info(const std::shared_ptr<VirtualFileServer> & /* server */,  const std::vector<std::tuple<ChannelId, std::string>>& /* names */) override;

                std::shared_ptr<directory_query_response_t>
                query_channel_directory(const std::shared_ptr<VirtualFileServer> & id, ChannelId channelId, const std::string &string) override;

                std::shared_ptr<ExecuteResponse<DirectoryModifyError>>
                create_channel_directory(const std::shared_ptr<VirtualFileServer> & id, ChannelId channelId, const std::string &string) override;

                std::shared_ptr<ExecuteResponse<FileDeleteError, FileDeleteResponse>>
                delete_channel_files(const std::shared_ptr<VirtualFileServer> & id, ChannelId channelId, const std::vector<std::string> &string) override;

                std::shared_ptr<ExecuteResponse<FileModifyError>>
                rename_channel_file(const std::shared_ptr<VirtualFileServer> & id, ChannelId channelId, const std::string &, ChannelId, const std::string &) override;

                std::shared_ptr<ExecuteResponse<FileInfoError, FileInfoResponse>>
                query_icon_info(const std::shared_ptr<VirtualFileServer> & /* server */, const std::vector<std::string>& /* names */) override;

                std::shared_ptr<directory_query_response_t> query_icon_directory(const std::shared_ptr<VirtualFileServer> & id) override;

                std::shared_ptr<ExecuteResponse<FileDeleteError, FileDeleteResponse>>
                delete_icons(const std::shared_ptr<VirtualFileServer> & id, const std::vector<std::string> &string) override;

                std::shared_ptr<ExecuteResponse<FileInfoError, FileInfoResponse>>
                query_avatar_info(const std::shared_ptr<VirtualFileServer> & /* server */, const std::vector<std::string>& /* names */) override;

                std::shared_ptr<directory_query_response_t> query_avatar_directory(const std::shared_ptr<VirtualFileServer> & id) override;

                std::shared_ptr<ExecuteResponse<FileDeleteError, FileDeleteResponse>>
                delete_avatars(const std::shared_ptr<VirtualFileServer> & id, const std::vector<std::string> &string) override;

            private:
                template <typename error_t, typename result_t = EmptyExecuteResponse>
                std::shared_ptr<ExecuteResponse<error_t, result_t>> create_execute_response() {
                    return std::make_shared<ExecuteResponse<error_t, result_t>>(this->result_notify_mutex, this->result_notify_cv);
                }

                std::shared_ptr<ExecuteResponse<FileInfoError, FileInfoResponse>> execute_info(const char* /* server unique id */, void* /* query */, size_t /* query count */);
                std::shared_ptr<directory_query_response_t> execute_query(const char* /* server unique id */, void* /* directory */);
                std::shared_ptr<ExecuteResponse<FileDeleteError, FileDeleteResponse>> execute_delete(const char* /* server unique id */, void* /* files */, size_t /* file count */);

                std::mutex result_notify_mutex{};
                std::condition_variable result_notify_cv{};
        };
    }

    namespace transfer {
        class RustFileTransfer : public AbstractProvider {
            public:

                std::shared_ptr<ExecuteResponse<TransferInitError, std::shared_ptr<Transfer>>>
                initialize_channel_transfer(Transfer::Direction direction,
                                            const std::shared_ptr<VirtualFileServer> &ptr, ChannelId id,
                                            const TransferInfo &info) override;

                std::shared_ptr<ExecuteResponse<TransferInitError, std::shared_ptr<Transfer>>>
                initialize_icon_transfer(Transfer::Direction direction, const std::shared_ptr<VirtualFileServer> &ptr,
                                         const TransferInfo &info) override;

                std::shared_ptr<ExecuteResponse<TransferInitError, std::shared_ptr<Transfer>>>
                initialize_avatar_transfer(Transfer::Direction direction, const std::shared_ptr<VirtualFileServer> &ptr,
                                           const TransferInfo &info) override;

                std::shared_ptr<ExecuteResponse<TransferListError, std::vector<ActiveFileTransfer>>>
                list_transfer() override;

                std::shared_ptr<ExecuteResponse<TransferActionError>>
                stop_transfer(const std::shared_ptr<VirtualFileServer> &ptr, transfer_id id, bool b) override;

            private:
                template <typename error_t, typename result_t = EmptyExecuteResponse>
                std::shared_ptr<ExecuteResponse<error_t, result_t>> create_execute_response() {
                    return std::make_shared<ExecuteResponse<error_t, result_t>>(this->result_notify_mutex, this->result_notify_cv);
                }

                std::mutex result_notify_mutex{};
                std::condition_variable result_notify_cv{};
        };
    }

    class RustVirtualFileServer : public VirtualFileServer {
        public:
            explicit RustVirtualFileServer(ServerId server_id, std::string unique_id) : VirtualFileServer{server_id, std::move(unique_id)} {}

            void max_networking_upload_bandwidth(int64_t value) override;
            void max_networking_download_bandwidth(int64_t value) override;
    };

    class RustFileProvider : public AbstractFileServer {
        public:
            RustFileProvider();
            virtual ~RustFileProvider();

            [[nodiscard]] std::string file_base_path() const override;

            filesystem::AbstractProvider &file_system() override;
            transfer::AbstractProvider &file_transfer() override;

            std::shared_ptr<VirtualFileServer> register_server(ServerId /* server id */) override;
            void unregister_server(ServerId /* server id */, bool /* delete files */) override;
        private:
            filesystem::RustFileSystem* file_system_;
            transfer::RustFileTransfer* file_transfer_;
    };
}