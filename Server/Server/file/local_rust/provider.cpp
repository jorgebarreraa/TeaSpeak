//
// Created by WolverinDEV on 20/04/2021.
//

#include <cassert>
#include <log/LogUtils.h>
#include "./provider.h"
#include "./imports.h"

using namespace ts::server;
using namespace ts::server::file;
using namespace ts::server::file::transfer;
using namespace ts::server::file::filesystem;

RustFileSystem::~RustFileSystem() = default;

std::shared_ptr<ExecuteResponse<ServerCommandError>> RustFileSystem::initialize_server(
        const std::shared_ptr<VirtualFileServer> &) {
    /* Nothing to do. */
    auto response = this->create_execute_response<ServerCommandError>();
    response->emplace_success();
    return response;
}

std::shared_ptr<ExecuteResponse<FileInfoError, FileInfoResponse>> RustFileSystem::query_channel_info(
        const std::shared_ptr<VirtualFileServer> &virtual_server, const std::vector<std::tuple<ChannelId, std::string>> &files) {
    std::vector<TeaFilePath> file_paths{};
    file_paths.reserve(files.size());
    for(const auto& [ channel_id, path ] : files) {
        file_paths.push_back(TeaFilePath{
            .type =  0,
            .channel_id =  channel_id,
            .path =  path.c_str()
        });
    }

    return this->execute_info(virtual_server->unique_id().c_str(), file_paths.data(), file_paths.size());
}

std::shared_ptr<RustFileSystem::directory_query_response_t> RustFileSystem::query_channel_directory(
        const std::shared_ptr<VirtualFileServer> &virtual_server, ChannelId channelId, const std::string &directory) {
    TeaFilePath path{
        .type = 0,
        .channel_id = channelId,
        .path = directory.c_str()
    };
    return this->execute_query(virtual_server->unique_id().c_str(), &path);
}

std::shared_ptr<ExecuteResponse<FileInfoError, FileInfoResponse>> RustFileSystem::query_icon_info(
        const std::shared_ptr<VirtualFileServer> &virtual_server, const std::vector<std::string> &files) {
    std::vector<TeaFilePath> file_paths{};
    file_paths.reserve(files.size());
    for(const auto& path : files) {
        file_paths.push_back(TeaFilePath{
                .type =  1,
                .channel_id =  0,
                .path =  path.c_str()
        });
    }

    return this->execute_info(virtual_server->unique_id().c_str(), file_paths.data(), file_paths.size());
}

std::shared_ptr<RustFileSystem::directory_query_response_t> RustFileSystem::query_icon_directory(
        const std::shared_ptr<VirtualFileServer> &virtual_server) {
    TeaFilePath path{
            .type = 1,
            .channel_id = 0,
            .path = nullptr
    };
    return this->execute_query(virtual_server->unique_id().c_str(), &path);
}

std::shared_ptr<ExecuteResponse<FileInfoError, FileInfoResponse>> RustFileSystem::query_avatar_info(
        const std::shared_ptr<VirtualFileServer> &virtual_server, const std::vector<std::string> &files) {
    std::vector<TeaFilePath> file_paths{};
    file_paths.reserve(files.size());
    for(const auto& path : files) {
        file_paths.push_back(TeaFilePath{
                .type =  2,
                .channel_id =  0,
                .path =  path.c_str()
        });
    }

    return this->execute_info(virtual_server->unique_id().c_str(), file_paths.data(), file_paths.size());
}

std::shared_ptr<RustFileSystem::directory_query_response_t> RustFileSystem::query_avatar_directory(
        const std::shared_ptr<VirtualFileServer> &virtual_server) {
    TeaFilePath path{
            .type = 2,
            .channel_id = 0,
            .path = nullptr
    };
    return this->execute_query(virtual_server->unique_id().c_str(), &path);
}

inline void path_info_to_directory_entry(DirectoryEntry& target, const TeaFilePathInfo& info) {
    switch (info.file_type) {
        case 1:
            target.type = DirectoryEntry::FILE;
            break;
        case 2:
            target.type = DirectoryEntry::DIRECTORY;
            break;

        default:
            target.type = DirectoryEntry::UNKNOWN;
            return;
    }

    assert(info.name);
    target.name = std::string{info.name};
    target.modified_at = std::chrono::system_clock::time_point{} + std::chrono::seconds{info.modify_timestamp};
    target.size = info.file_size;
    target.empty = info.directory_empty;
}

std::shared_ptr<ExecuteResponse<FileInfoError, FileInfoResponse>> RustFileSystem::execute_info(const char *server_unique_id, void *paths,
                                                                                               size_t paths_count) {
    auto response = this->create_execute_response<FileInfoError, FileInfoResponse>();

    const TeaFilePathInfo* result{nullptr};
    auto error_ptr = libteaspeak_file_system_query_file_info(server_unique_id, (const TeaFilePath*) paths, paths_count, &result);
    if(error_ptr) {
        libteaspeak_file_free_file_info(result);

        response->emplace_fail(FileInfoErrorType::UNKNOWN, error_ptr);
        libteaspeak_free_str(error_ptr);
    } else {
        assert(result);
        std::vector<FileInfoResponse::FileInfo> file_infos{};
        file_infos.reserve(paths_count);

        auto info_ptr{result};
        while(info_ptr->query_result >= 0) {
            auto& info = file_infos.emplace_back();
            switch (info_ptr->query_result) {
                case 0:
                    /* success */
                    info.status = FileInfoResponse::StatusType::SUCCESS;
                    path_info_to_directory_entry(info.info, *info_ptr);
                    break;

                case 1:
                    info.status = FileInfoResponse::StatusType::PATH_EXCEEDS_ROOT_PATH;
                    break;

                case 2:
                    info.status = FileInfoResponse::StatusType::PATH_DOES_NOT_EXISTS;
                    break;

                case 3:
                    info.status = FileInfoResponse::StatusType::UNKNOWN_FILE_TYPE;
                    break;

                default:
                    info.status = FileInfoResponse::StatusType::FAILED_TO_QUERY_INFO;
                    info.error_detail = std::string{"invalid query result "} + std::to_string(info_ptr->query_result);
                    break;
            }

            info_ptr++;
        }

        assert(file_infos.size() == paths_count);
        libteaspeak_file_free_file_info(result);
        response->emplace_success(FileInfoResponse{file_infos});
    }

    return response;
}

std::shared_ptr<RustFileSystem::directory_query_response_t> RustFileSystem::execute_query(const char *server_unique_id, void *path) {
    auto response = this->create_execute_response<DirectoryQueryError, std::deque<DirectoryEntry>>();

    const TeaFilePathInfo* result{nullptr};
    auto error_ptr = libteaspeak_file_system_query_directory(server_unique_id, (const TeaFilePath*) path, &result);
    if(error_ptr) {
        libteaspeak_file_free_file_info(result);

        response->emplace_fail(DirectoryQueryErrorType::UNKNOWN, error_ptr);
        libteaspeak_free_str(error_ptr);
    } else {
        assert(result);
        /* result->query_result could be zero or minus one in both cases we're iterating the query result */
        if(result->query_result > 0) {
            /* An error occurred */
            switch(result->query_result) {
                case 1:
                    response->emplace_fail(DirectoryQueryErrorType::PATH_EXCEEDS_ROOT_PATH, "");
                    break;

                case 2:
                    response->emplace_fail(DirectoryQueryErrorType::PATH_DOES_NOT_EXISTS, "");
                    break;

                case 4:
                    response->emplace_fail(DirectoryQueryErrorType::PATH_IS_A_FILE, "");
                    break;

                case 5:
                    response->emplace_fail(DirectoryQueryErrorType::UNKNOWN, "io error");
                    break;

                default:
                    response->emplace_fail(DirectoryQueryErrorType::UNKNOWN, std::string{"unknown error result "} + std::to_string(result->query_result));
                    break;
            }
        } else {
            std::deque<DirectoryEntry> entries{};

            auto info_ptr{result};
            while(info_ptr->query_result >= 0) {
                assert(info_ptr->query_result == 0);
                path_info_to_directory_entry(entries.emplace_back(), *info_ptr);
                info_ptr++;
            }

            response->emplace_success(std::move(entries));
        }
        libteaspeak_file_free_file_info(result);
    }
    return response;
}

std::shared_ptr<ExecuteResponse<FileDeleteError, FileDeleteResponse>> RustFileSystem::delete_channel_files(
        const std::shared_ptr<VirtualFileServer> &virtual_server, ChannelId channel_id, const std::vector<std::string> &files) {
    std::vector<TeaFilePath> file_paths{};
    file_paths.reserve(files.size());
    for(const auto& path : files) {
        file_paths.push_back(TeaFilePath{
                .type =  0,
                .channel_id =  channel_id,
                .path =  path.c_str()
        });
    }

    return this->execute_delete(virtual_server->unique_id().c_str(), file_paths.data(), file_paths.size());
}

std::shared_ptr<ExecuteResponse<FileDeleteError, FileDeleteResponse>> RustFileSystem::delete_icons(
        const std::shared_ptr<VirtualFileServer> &virtual_server, const std::vector<std::string> &files) {
    std::vector<TeaFilePath> file_paths{};
    file_paths.reserve(files.size());
    for(const auto& path : files) {
        file_paths.push_back(TeaFilePath{
                .type =  1,
                .channel_id =  0,
                .path =  path.c_str()
        });
    }

    return this->execute_delete(virtual_server->unique_id().c_str(), file_paths.data(), file_paths.size());
}

std::shared_ptr<ExecuteResponse<FileDeleteError, FileDeleteResponse>> RustFileSystem::delete_avatars(
        const std::shared_ptr<VirtualFileServer> &virtual_server, const std::vector<std::string> &files) {
    std::vector<TeaFilePath> file_paths{};
    file_paths.reserve(files.size());
    for(const auto& path : files) {
        file_paths.push_back(TeaFilePath{
                .type =  2,
                .channel_id =  0,
                .path =  path.c_str()
        });
    }

    return this->execute_delete(virtual_server->unique_id().c_str(), file_paths.data(), file_paths.size());
}

inline void path_info_to_delete_result(FileDeleteResponse::DeleteResult& result, const TeaFilePathInfo& info) {
    switch (info.query_result) {
        case 0:
            result.status = FileDeleteResponse::StatusType::SUCCESS;
            break;

        case 1:
            result.status = FileDeleteResponse::StatusType::PATH_EXCEEDS_ROOT_PATH;
            break;

        case 2:
            result.status = FileDeleteResponse::StatusType::PATH_DOES_NOT_EXISTS;
            break;

        case 5:
            result.status = FileDeleteResponse::StatusType::FAILED_TO_DELETE_FILES;
            break;

        case 6:
            result.status = FileDeleteResponse::StatusType::SOME_FILES_ARE_LOCKED;
            break;

        default:
            result.status = FileDeleteResponse::StatusType::FAILED_TO_DELETE_FILES;
            result.error_detail = std::string{"unknown delete error "} + std::to_string(info.query_result);
            break;
    }
}

std::shared_ptr<ExecuteResponse<FileDeleteError, FileDeleteResponse>> RustFileSystem::execute_delete(const char *server_unique_id, void *paths,
                                                                                                     size_t paths_count) {
    auto response = this->create_execute_response<FileDeleteError, FileDeleteResponse>();

    const TeaFilePathInfo* result{nullptr};
    auto error_ptr = libteaspeak_file_system_delete_files(server_unique_id, (const TeaFilePath*) paths, paths_count, &result);
    if(error_ptr) {
        libteaspeak_file_free_file_info(result);

        response->emplace_fail(FileDeleteErrorType::UNKNOWN, error_ptr);
        libteaspeak_free_str(error_ptr);
    } else {
        assert(result);
        std::vector<FileDeleteResponse::DeleteResult> results{};
        results.reserve(paths_count);

        auto info_ptr{result};
        while(info_ptr->query_result >= 0) {
            path_info_to_delete_result(results.emplace_back(), *info_ptr);
            info_ptr++;
        }

        assert(results.size() == paths_count);
        libteaspeak_file_free_file_info(result);
        response->emplace_success(FileDeleteResponse{results});
    }

    return response;
}

std::shared_ptr<ExecuteResponse<DirectoryModifyError>> RustFileSystem::create_channel_directory(
        const std::shared_ptr<VirtualFileServer> &virtual_server, ChannelId channelId, const std::string &path) {
    auto response = this->create_execute_response<DirectoryModifyError>();
    auto result = libteaspeak_file_system_create_channel_directory(virtual_server->unique_id().c_str(), channelId, path.c_str());
    switch (result) {
        case 0:
            response->emplace_success();
            break;

        case 1:
            response->emplace_fail(DirectoryModifyErrorType::UNKNOWN, "server not found");
            break;

        case 2:
            response->emplace_fail(DirectoryModifyErrorType::PATH_EXCEEDS_ROOT_PATH, "");
            break;

        case 3:
            response->emplace_fail(DirectoryModifyErrorType::PATH_ALREADY_EXISTS, "");
            break;

        case 4:
            response->emplace_fail(DirectoryModifyErrorType::FAILED_TO_CREATE_DIRECTORIES, "");
            break;

        default:
            response->emplace_fail(DirectoryModifyErrorType::UNKNOWN, std::string{"invalid return code "} + std::to_string(result));
            break;
    }

    return response;
}

std::shared_ptr<ExecuteResponse<FileModifyError>> RustFileSystem::rename_channel_file(
        const std::shared_ptr<VirtualFileServer> &virtual_server, ChannelId old_channel_id, const std::string &old_path, ChannelId new_channel_id,
        const std::string &new_path) {
    auto response = this->create_execute_response<FileModifyError>();
    auto result = libteaspeak_file_system_rename_channel_file(virtual_server->unique_id().c_str(), old_channel_id, old_path.c_str(), new_channel_id, new_path.c_str());
    switch (result) {
        case 0:
            response->emplace_success();
            break;

        case 1:
            response->emplace_fail(FileModifyErrorType::UNKNOWN, "server not found");
            break;

        case 2:
            response->emplace_fail(FileModifyErrorType::UNKNOWN, "invalid path types");
            break;

        case 3:
            response->emplace_fail(FileModifyErrorType::PATH_EXCEEDS_ROOT_PATH, "");
            break;

        case 4:
            response->emplace_fail(FileModifyErrorType::TARGET_PATH_EXCEEDS_ROOT_PATH, "");
            break;

        case 5:
            response->emplace_fail(FileModifyErrorType::PATH_DOES_NOT_EXISTS, "");
            break;

        case 6:
            response->emplace_fail(FileModifyErrorType::SOME_FILES_ARE_LOCKED, "");
            break;

        case 7:
            response->emplace_fail(FileModifyErrorType::TARGET_PATH_ALREADY_EXISTS, "");
            break;

        case 8:
            response->emplace_fail(FileModifyErrorType::FAILED_TO_RENAME_FILE, "");
            break;

        default:
            response->emplace_fail(FileModifyErrorType::UNKNOWN, std::string{"invalid return code "} + std::to_string(result));
            break;
    }

    return response;
}

std::shared_ptr<ExecuteResponse<TransferInitError, std::shared_ptr<Transfer>>>
RustFileTransfer::initialize_channel_transfer(transfer::Transfer::Direction direction,
                                                        const std::shared_ptr<VirtualFileServer> &ptr, ChannelId id,
                                                        const transfer::AbstractProvider::TransferInfo &info) {
    auto result = this->create_execute_response<TransferInitError, std::shared_ptr<Transfer>>();
    result->emplace_fail(TransferInitError::Type::UNKNOWN, "");
    return result;
}

std::shared_ptr<ExecuteResponse<TransferInitError, std::shared_ptr<Transfer>>>
RustFileTransfer::initialize_icon_transfer(transfer::Transfer::Direction direction,
                                                     const std::shared_ptr<VirtualFileServer> &ptr,
                                                     const transfer::AbstractProvider::TransferInfo &info) {
    auto result = this->create_execute_response<TransferInitError, std::shared_ptr<Transfer>>();
    result->emplace_fail(TransferInitError::Type::UNKNOWN, "");
    return result;
}

std::shared_ptr<ExecuteResponse<TransferInitError, std::shared_ptr<Transfer>>>
RustFileTransfer::initialize_avatar_transfer(transfer::Transfer::Direction direction,
                                                       const std::shared_ptr<VirtualFileServer> &ptr,
                                                       const transfer::AbstractProvider::TransferInfo &info) {
    auto result = this->create_execute_response<TransferInitError, std::shared_ptr<Transfer>>();
    result->emplace_fail(TransferInitError::Type::UNKNOWN, "");
    return result;
}

std::shared_ptr<ExecuteResponse<TransferListError, std::vector<ActiveFileTransfer>>>
RustFileTransfer::list_transfer() {
    auto result = this->create_execute_response<TransferListError, std::vector<ActiveFileTransfer>>();
    result->emplace_fail(TransferListError::UNKNOWN);
    return result;
}

std::shared_ptr<ExecuteResponse<TransferActionError>>
RustFileTransfer::stop_transfer(const std::shared_ptr<VirtualFileServer> &ptr, transfer::transfer_id id,
                                          bool b) {
    auto result = this->create_execute_response<TransferActionError>();
    result->emplace_fail(TransferActionError{TransferActionError::Type::UNKNOWN_TRANSFER});
    return result;
}

RustFileProvider::RustFileProvider() :
    file_system_{new RustFileSystem{}},
    file_transfer_{new RustFileTransfer{}}
{}

RustFileProvider::~RustFileProvider() {
    delete this->file_transfer_;
    delete this->file_system_;
}

std::string RustFileProvider::file_base_path() const {
    return "TODO!"; /* FIXME! */
}

filesystem::AbstractProvider & RustFileProvider::file_system() {
    return *this->file_system_;
}

transfer::AbstractProvider & RustFileProvider::file_transfer() {
    return *this->file_transfer_;
}

std::shared_ptr<VirtualFileServer> RustFileProvider::register_server(ServerId server_id) {
    auto server = this->find_virtual_server(server_id);
    if(server) return server;

    server = std::make_shared<file::RustVirtualFileServer>(server_id, std::to_string(server_id));
    libteaspeak_file_system_register_server(server->unique_id().c_str());
    {
        std::lock_guard slock{this->servers_mutex};
        this->servers_.push_back(server);
    }

    return server;
}

void RustFileProvider::unregister_server(ServerId server_id, bool delete_files) {
    auto server_unique_id = std::to_string(server_id);
    std::shared_ptr<VirtualFileServer> server{};

    {
        std::lock_guard slock{this->servers_mutex};
        auto it = std::find_if(this->servers_.begin(), this->servers_.end(), [&](const std::shared_ptr<VirtualFileServer>& server) {
            return server->unique_id() == server_unique_id;
        });

        if(it == this->servers_.end()) {
            return;
        }

        server = *it;
        this->servers_.erase(it);
    }

    libteaspeak_file_system_unregister_server(server->unique_id().c_str(), delete_files);
}

void RustVirtualFileServer::max_networking_download_bandwidth(int64_t value) {
    VirtualFileServer::max_networking_download_bandwidth(value);
}

void RustVirtualFileServer::max_networking_upload_bandwidth(int64_t value) {
    VirtualFileServer::max_networking_upload_bandwidth(value);
}



#define log(...)                    \
switch(level) {                     \
    case 0:                         \
        logTrace(__VA_ARGS__);      \
        break;                      \
    case 1:                         \
        debugMessage(__VA_ARGS__);  \
        break;                      \
    case 2:                         \
        logMessage(__VA_ARGS__);    \
        break;                      \
    case 3:                         \
        logWarning(__VA_ARGS__);    \
        break;                      \
    case 4:                         \
        logError(__VA_ARGS__);      \
        break;                      \
    case 5:                         \
        logCritical(__VA_ARGS__);   \
        break;                      \
    default:                        \
        debugMessage(__VA_ARGS__);  \
        break;                      \
}

void libteaspeak_file_callback_log(uint8_t level, const void* callback_data_ptr, const char* message_ptr, uint32_t length) {
    std::string_view message{message_ptr, length};
    log(LOG_GENERAL, "{}", message);
}

#undef log

static TeaFileNativeCallbacks native_callbacks{
    .version = 1,

    .log = libteaspeak_file_callback_log,
};

std::shared_ptr<AbstractFileServer> file_instance{};
bool file::initialize(std::string& error, const std::string& host_names, uint16_t port) {
    logMessage(LOG_FT, "Initializing file server with version {}", libteaspeak_file_version());

    auto error_ptr = libteaspeak_file_initialize(&native_callbacks, sizeof native_callbacks);
    if(error_ptr) {
        error = error_ptr;
        libteaspeak_free_str(error_ptr);
        return false;
    }

    /* FIXME: Start server */

    file_instance = std::make_shared<RustFileProvider>();
    return true;
}

void file::finalize() {
    file_instance = nullptr;
    libteaspeak_file_finalize();
}

std::shared_ptr<AbstractFileServer> file::server() {
    return file_instance;
}
