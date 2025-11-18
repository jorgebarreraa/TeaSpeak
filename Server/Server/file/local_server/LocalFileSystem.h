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

namespace ts::server::file::filesystem {
#ifdef FS_INCLUDED
    namespace fs = std::experimental::filesystem;
#endif

    class LocalFileSystem : public filesystem::AbstractProvider {
            using FileModifyError = filesystem::FileModifyError;
            using DirectoryModifyError = filesystem::DirectoryModifyError;
        public:
            enum struct FileCategory {
                ICON,
                AVATAR,
                CHANNEL
            };

            virtual ~LocalFileSystem();

            bool initialize(std::string & /* error */, const std::string & /* root path */);
            void finalize();

            void lock_file(const std::string& /* absolute path */);
            void unlock_file(const std::string& /* absolute path */);

            [[nodiscard]] inline const auto &root_path() const { return this->root_path_; }

            [[nodiscard]] std::string absolute_avatar_path(const std::shared_ptr<VirtualFileServer> &, const std::string&);
            [[nodiscard]] std::string absolute_icon_path(const std::shared_ptr<VirtualFileServer> &, const std::string&);
            [[nodiscard]] std::string absolute_channel_path(const std::shared_ptr<VirtualFileServer> &, ChannelId, const std::string&);

            std::shared_ptr<ExecuteResponse<ServerCommandError>> initialize_server(const std::shared_ptr<VirtualFileServer> & /* server */) override;
            std::shared_ptr<ExecuteResponse<ServerCommandError>> delete_server(const std::shared_ptr<VirtualFileServer> & /* server */);

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
#ifdef FS_INCLUDED
            [[nodiscard]] fs::path server_path(const std::shared_ptr<VirtualFileServer> &);
            [[nodiscard]] fs::path server_channel_path(const std::shared_ptr<VirtualFileServer> &, ChannelId);
            [[nodiscard]] static bool exceeds_base_path(const fs::path& /* base */, const fs::path& /* target */);
            [[nodiscard]] bool is_any_file_locked(const fs::path& /* base */, const std::string& /* path */, std::string& /* file (relative to the base) */);

            [[nodiscard]] std::shared_ptr<ExecuteResponse<FileDeleteError, FileDeleteResponse>>
            delete_files(const fs::path& /* base */, const std::vector<std::string> &string);

            [[nodiscard]] std::shared_ptr<directory_query_response_t>
            query_directory(const fs::path& /* base */, const std::string &string, bool);

            [[nodiscard]] std::shared_ptr<ExecuteResponse<FileInfoError, FileInfoResponse>>
            query_file_info(const std::vector<std::tuple<fs::path, std::string>> &string);
#endif

            template <typename error_t, typename result_t = EmptyExecuteResponse>
            std::shared_ptr<ExecuteResponse<error_t, result_t>> create_execute_response() {
                return std::make_shared<ExecuteResponse<error_t, result_t>>(this->result_notify_mutex, this->result_notify_cv);
            }
            std::string target_file_path(FileCategory type, const std::shared_ptr<VirtualFileServer> &sid, ts::ChannelId cid, const std::string &path);

            std::mutex result_notify_mutex{};
            std::condition_variable result_notify_cv{};

            std::string root_path_{};

            std::mutex locked_files_mutex{};
            std::deque<std::string> locked_files_{};
    };
}