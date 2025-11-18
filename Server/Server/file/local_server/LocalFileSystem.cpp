//
// Created by WolverinDEV on 29/04/2020.
//
#include <experimental/filesystem>
#define FS_INCLUDED

#include <log/LogUtils.h>
#include "./LocalFileSystem.h"
#include "./clnpath.h"

using namespace ts::server::file;
using namespace ts::server::file::filesystem;
namespace fs = std::experimental::filesystem;
using directory_query_response_t = AbstractProvider::directory_query_response_t;

LocalFileSystem::~LocalFileSystem() = default;

bool LocalFileSystem::initialize(std::string &error_message, const std::string &root_path_string) {
    auto root_path = fs::u8path(root_path_string);
    std::error_code error{};

    if(!fs::exists(root_path, error)) {
        if(error)
            logWarning(LOG_FT, "Failed to check root path existence. Assuming it does not exist. ({}/{})", error.value(), error.message());
        if(!fs::create_directories(root_path, error) || error) {
            error_message = "Failed to create root file system at " + root_path_string + ": " + std::to_string(error.value()) + "/" + error.message();
            return false;
        }
    }

    auto croot = clnpath(fs::absolute(root_path).string());
    logMessage(LOG_FT, "Started file system root at {}", croot);
    this->root_path_ = croot;
    return true;
}

void LocalFileSystem::finalize() {}

fs::path LocalFileSystem::server_path(const std::shared_ptr<VirtualFileServer> &server) {
    return fs::u8path(this->root_path_) / fs::u8path("server_" + std::to_string(server->server_id()));
}

fs::path LocalFileSystem::server_channel_path(const std::shared_ptr<VirtualFileServer> &server, ts::ChannelId cid) {
    return this->server_path(server) / fs::u8path("channel_" + std::to_string(cid));
}

bool LocalFileSystem::exceeds_base_path(const fs::path &base, const fs::path &target) {
    auto rel_target = clnpath(target.string());
    if(rel_target.starts_with("..")) return true;

    auto base_string = clnpath(fs::absolute(base).string());
    auto target_string = clnpath(fs::absolute(target).string());
    return !target_string.starts_with(base_string);
}

bool LocalFileSystem::is_any_file_locked(const fs::path &base, const std::string &path, std::string &locked_file) {
    auto c_path = clnpath(fs::absolute(base / fs::u8path(path)).string());

    std::lock_guard lock{this->locked_files_mutex};
    for(const auto& lfile : this->locked_files_) {
        if(lfile.starts_with(c_path)) {
            locked_file = lfile.substr(base.string().length());
            return true;
        }
    }

    return false;
}

std::string LocalFileSystem::target_file_path(FileCategory type, const std::shared_ptr<VirtualFileServer> &server, ts::ChannelId cid, const std::string &path) {
    fs::path target_path{};
    switch (type) {
        case FileCategory::CHANNEL:
            target_path = this->server_channel_path(server, cid) / path;
            break;
        case FileCategory::ICON:
            target_path = this->server_path(server) / "icons" / path;
            break;
        case FileCategory::AVATAR:
            target_path = this->server_path(server) / "avatars" / path;
            break;
    }

    return clnpath(fs::absolute(target_path).string());
}

std::string LocalFileSystem::absolute_avatar_path(const std::shared_ptr<VirtualFileServer> &sid, const std::string &path) {
    return this->target_file_path(FileCategory::AVATAR, sid, 0, path);
}

std::string LocalFileSystem::absolute_icon_path(const std::shared_ptr<VirtualFileServer> &sid, const std::string &path) {
    return this->target_file_path(FileCategory::ICON, sid, 0, path);
}

std::string LocalFileSystem::absolute_channel_path(const std::shared_ptr<VirtualFileServer> &sid, ChannelId cid, const std::string &path) {
    return this->target_file_path(FileCategory::CHANNEL, sid, cid, path);
}

void LocalFileSystem::lock_file(const std::string &c_path) {
    std::lock_guard lock{this->locked_files_mutex};
    this->locked_files_.push_back(c_path);
}

void LocalFileSystem::unlock_file(const std::string &c_path) {
    std::lock_guard lock{this->locked_files_mutex};

    this->locked_files_.erase(std::remove_if(this->locked_files_.begin(), this->locked_files_.end(), [&](const auto& p) { return p == c_path; }), this->locked_files_.end());
}

std::shared_ptr<ExecuteResponse<ServerCommandError>> LocalFileSystem::initialize_server(const std::shared_ptr<VirtualFileServer> &id) {
    auto path = this->server_path(id);
    std::error_code error{};

    auto response = this->create_execute_response<ServerCommandError>();

    if(!fs::exists(path, error)) {
        if(!fs::create_directories(path, error) || error) {
            response->emplace_fail(ServerCommandErrorType::FAILED_TO_CREATE_DIRECTORIES, std::to_string(error.value()) + "/" + error.message());
            return response;
        }
    }

    //TODO: Copy the default icon

    response->emplace_success();
    return response;
}

std::shared_ptr<ExecuteResponse<ServerCommandError>> LocalFileSystem::delete_server(const std::shared_ptr<VirtualFileServer> &id) {
    auto path = this->server_path(id);
    std::error_code error{};

    auto response = this->create_execute_response<ServerCommandError>();

    //TODO: Stop all running file transfers!

    if(fs::exists(path, error)) {
        if(!fs::remove_all(path, error) || error) {
            response->emplace_fail(ServerCommandErrorType::FAILED_TO_DELETE_DIRECTORIES, std::to_string(error.value()) + "/" + error.message());
            return response;
        }
    }

    response->emplace_success();
    return response;
}

std::shared_ptr<directory_query_response_t> LocalFileSystem::query_directory(const fs::path &base,
                                                                             const std::string &path,
                                                                             bool allow_non_existance) {
    std::error_code error{};
    auto response = this->create_execute_response<DirectoryQueryError, std::deque<DirectoryEntry>>();
    auto target_path = base / fs::u8path(path);
    if(this->exceeds_base_path(base, target_path)) {
        response->emplace_fail(DirectoryQueryErrorType::PATH_EXCEEDS_ROOT_PATH, "");
        return response;
    }

    if(!fs::exists(target_path, error)) {
        if(allow_non_existance)
            response->emplace_success();
        else
            response->emplace_fail(DirectoryQueryErrorType::PATH_DOES_NOT_EXISTS, "");
        return response;
    } else if(error) {
        logWarning(LOG_FT, "Failed to check for file at {}: {}. Assuming it does not exists.", target_path.string(), error.value(), error.message());
        response->emplace_fail(DirectoryQueryErrorType::PATH_DOES_NOT_EXISTS, "");
        return response;
    }

    if(!fs::is_directory(target_path, error)) {
        response->emplace_fail(DirectoryQueryErrorType::PATH_IS_A_FILE, "");
        return response;
    } else if(error) {
        logWarning(LOG_FT, "Failed to check for directory at {}: {}. Assuming its not a directory.", target_path.string(), error.value(), error.message());
        response->emplace_fail(DirectoryQueryErrorType::PATH_IS_A_FILE, "");
        return response;
    }

    std::deque<DirectoryEntry> entries{};
    for(auto& entry : fs::directory_iterator(target_path, error)) {
        auto status = entry.status(error);
        if(error) {
            logWarning(LOG_FT, "Failed to query file status for {} ({}/{}). Skipping entry for directory query.", entry.path().string(), error.value(), error.message());
            continue;
        }

        if(status.type() == fs::file_type::directory) {
            auto& dentry = entries.emplace_back();
            dentry.type = DirectoryEntry::DIRECTORY;
            dentry.name = entry.path().filename();

            dentry.empty = fs::is_empty(entry.path(), error);
            if(error)
                logWarning(LOG_FT, "Failed to query directory empty state for directory {} ({}/{})", target_path.string(), error.value(), error.message());

            dentry.modified_at = fs::last_write_time(entry.path(), error);
            if(error)
                logWarning(LOG_FT, "Failed to query last write time for directory {} ({}/{})", entry.path().string(), error.value(), error.message());
            dentry.size = 0;
        } else if(status.type() == fs::file_type::regular) {
            auto& dentry = entries.emplace_back();
            dentry.type = DirectoryEntry::FILE;
            dentry.name = entry.path().filename();

            dentry.modified_at = fs::last_write_time(entry.path(), error);
            if(error)
                logWarning(LOG_FT, "Failed to query last write time for file {} ({}/{}).", entry.path().string(), error.value(), error.message());
            dentry.size = fs::file_size(entry.path(), error);
            if(error)
                logWarning(LOG_FT, "Failed to query size for file {} ({}/{}).", entry.path().string(), error.value(), error.message());
        } else {
            logWarning(LOG_FT, "Directory query listed an unknown file type for file {} ({}).", entry.path().string(), (int) status.type());
        }
    }
    if(error && entries.empty()) {
        response->emplace_fail(DirectoryQueryErrorType::FAILED_TO_LIST_FILES, std::to_string(error.value()) + "/" + error.message());
        return response;
    }
    response->emplace_success(std::forward<decltype(entries)>(entries));
    return response;
}

std::shared_ptr<directory_query_response_t> LocalFileSystem::query_icon_directory(const std::shared_ptr<VirtualFileServer> &id) {
    return this->query_directory(this->server_path(id) / fs::u8path("icons"), "/", true);
}

std::shared_ptr<directory_query_response_t> LocalFileSystem::query_avatar_directory(const std::shared_ptr<VirtualFileServer> &id) {
    return this->query_directory(this->server_path(id) / fs::u8path("avatars"), "/", true);
}

std::shared_ptr<directory_query_response_t> LocalFileSystem::query_channel_directory(const std::shared_ptr<VirtualFileServer> &id, ChannelId channelId, const std::string &path) {
    return this->query_directory(this->server_channel_path(id, channelId), path, false);
}

std::shared_ptr<ExecuteResponse<DirectoryModifyError>> LocalFileSystem::create_channel_directory(const std::shared_ptr<VirtualFileServer> &id, ChannelId channelId, const std::string &path) {
    auto channel_path_root = this->server_channel_path(id, channelId);
    std::error_code error{};

    auto response = this->create_execute_response<DirectoryModifyError>();
    auto target_path = channel_path_root / fs::u8path(path);
    if(this->exceeds_base_path(channel_path_root, target_path)) {
        response->emplace_fail(DirectoryModifyErrorType::PATH_EXCEEDS_ROOT_PATH, "");
        return response;
    }

    if(fs::exists(target_path, error)) {
        response->emplace_fail(DirectoryModifyErrorType::PATH_ALREADY_EXISTS, "");
        return response;
    } else if(error) {
        logWarning(LOG_FT, "Failed to check for file at {}: {}. Assuming it does not exists.", target_path.string(), error.value(), error.message());
    }

    if(!fs::create_directories(target_path, error) || error) {
        response->emplace_fail(DirectoryModifyErrorType::FAILED_TO_CREATE_DIRECTORIES, std::to_string(error.value()) + "/" + error.message());
        return response;
    }

    response->emplace_success();
    return response;
}

std::shared_ptr<ExecuteResponse<FileModifyError>> LocalFileSystem::rename_channel_file(const std::shared_ptr<VirtualFileServer> &id, ChannelId channelId, const std::string &current_path_string, ChannelId targetChannelId, const std::string &new_path_string) {
    auto channel_path_root = this->server_channel_path(id, channelId);
    auto target_path_root = this->server_channel_path(id, targetChannelId);
    std::error_code error{};
    std::string locked_file{};

    auto response = this->create_execute_response<FileModifyError>();
    auto current_path = channel_path_root / fs::u8path(current_path_string);
    auto target_path = target_path_root / fs::u8path(new_path_string);

    if(this->exceeds_base_path(channel_path_root, current_path)) {
        response->emplace_fail(FileModifyErrorType::PATH_EXCEEDS_ROOT_PATH, "");
        return response;
    }
    if(this->exceeds_base_path(target_path_root, target_path)) {
        response->emplace_fail(FileModifyErrorType::TARGET_PATH_EXCEEDS_ROOT_PATH, "");
        return response;
    }

    if(!fs::exists(current_path, error)) {
        response->emplace_fail(FileModifyErrorType::PATH_DOES_NOT_EXISTS, "");
        return response;
    } else if(error) {
        logWarning(LOG_FT, "Failed to check for file at {}: {}. Assuming it does not exists.", current_path.string(), error.value(), error.message());
        response->emplace_fail(FileModifyErrorType::PATH_DOES_NOT_EXISTS, "");
        return response;
    }

    if(!fs::exists(target_path.parent_path(), error)) {
        if(!fs::create_directories(target_path.parent_path(), error)) {
            response->emplace_fail(FileModifyErrorType::FAILED_TO_CREATE_DIRECTORIES, std::to_string(error.value()) + "/" + error.message());
            return response;
        }
    } else if(error) {
        logWarning(LOG_FT, "Failed to test for target directory existence for {}: {}/{}. Assuming it does exists", target_path.parent_path().string(), error.value(), error.message());
    }

    if(fs::exists(target_path, error)) {
        response->emplace_fail(FileModifyErrorType::TARGET_PATH_ALREADY_EXISTS, "");
        return response;
    } else if(error) {
        logWarning(LOG_FT, "Failed to check for file at {}: {}. Assuming it does exists.", current_path.string(), error.value(), error.message());
        response->emplace_fail(FileModifyErrorType::TARGET_PATH_ALREADY_EXISTS, "");
        return response;
    }

    if(this->is_any_file_locked(channel_path_root, current_path, locked_file)) {
        response->emplace_fail(FileModifyErrorType::SOME_FILES_ARE_LOCKED, locked_file);
        return response;
    }

    fs::rename(current_path, target_path, error);
    if(error) {
        response->emplace_fail(FileModifyErrorType::FAILED_TO_RENAME_FILE, std::to_string(error.value()) + "/" + error.message());
        return response;
    }

    response->emplace_success();
    return response;
}

std::shared_ptr<ExecuteResponse<FileDeleteError, FileDeleteResponse>> LocalFileSystem::delete_files(const fs::path &base,
                                                                               const std::vector<std::string> &paths) {
    std::error_code error{};
    std::string locked_file{};
    auto response = this->create_execute_response<FileDeleteError, FileDeleteResponse>();

    std::vector<FileDeleteResponse::DeleteResult> delete_results{};
    for(const auto& path : paths) {
        auto target_path = base / fs::u8path(path);

        if(!fs::exists(target_path, error)) {
            delete_results.emplace_back(FileDeleteResponse::StatusType::PATH_DOES_NOT_EXISTS, "");
            continue;
        } else if(error) {
            logWarning(LOG_FT, "Failed to check for file at {}: {}. Assuming it does exists.", target_path.string(), error.value(), error.message());
            delete_results.emplace_back(FileDeleteResponse::StatusType::PATH_DOES_NOT_EXISTS, "");
            continue;
        }

        if(this->is_any_file_locked(base, path, locked_file)) {
            delete_results.emplace_back(FileDeleteResponse::StatusType::SOME_FILES_ARE_LOCKED, locked_file);
            continue;
        }

        if(!fs::remove_all(target_path, error) || error) {
            delete_results.emplace_back(FileDeleteResponse::StatusType::FAILED_TO_DELETE_FILES, std::to_string(error.value()) + "/" + error.message());
            continue;
        }

        delete_results.emplace_back(FileDeleteResponse::StatusType::SUCCESS, "");
    }

    response->emplace_success(FileDeleteResponse{delete_results});
    return response;
}

std::shared_ptr<ExecuteResponse<FileDeleteError, FileDeleteResponse>> LocalFileSystem::delete_channel_files(const std::shared_ptr<VirtualFileServer> &id, ChannelId channelId, const std::vector<std::string> &path) {
    return this->delete_files(this->server_channel_path(id, channelId), path);
}

std::shared_ptr<ExecuteResponse<FileDeleteError, FileDeleteResponse>> LocalFileSystem::delete_icons(const std::shared_ptr<VirtualFileServer> &id, const std::vector<std::string> &icon) {
    return this->delete_files(this->server_path(id) / fs::u8path("icons"), icon);
}

std::shared_ptr<ExecuteResponse<FileDeleteError, FileDeleteResponse>> LocalFileSystem::delete_avatars(const std::shared_ptr<VirtualFileServer> &id, const std::vector<std::string> &avatar) {
    return this->delete_files(this->server_path(id) / fs::u8path("avatars"), avatar);
}

std::shared_ptr<ExecuteResponse<FileInfoError, FileInfoResponse>> LocalFileSystem::query_file_info(const std::vector<std::tuple<fs::path, std::string>> &paths) {
    std::error_code error{};
    auto response = this->create_execute_response<FileInfoError, FileInfoResponse>();
    std::vector<FileInfoResponse::FileInfo> file_infos{};
    file_infos.reserve(paths.size());

    for(const auto& [base, path] : paths) {
        auto target_path = base / fs::u8path(path);
        if(this->exceeds_base_path(base, target_path)) {
            file_infos.emplace_back(FileInfoResponse::StatusType::PATH_EXCEEDS_ROOT_PATH, "", DirectoryEntry{});
            continue;
        }

        if(!fs::exists(target_path, error)) {
            file_infos.emplace_back(FileInfoResponse::StatusType::PATH_DOES_NOT_EXISTS, "", DirectoryEntry{});
            continue;
        } else if(error) {
            logWarning(LOG_FT, "Failed to check for file at {}: {}. Assuming it does not exists.", target_path.string(), error.value(), error.message());

            file_infos.emplace_back(FileInfoResponse::StatusType::PATH_DOES_NOT_EXISTS, "", DirectoryEntry{});
            continue;
        }

        auto status = fs::status(target_path, error);
        if(error) {
            logWarning(LOG_FT, "Failed to query file status for {} ({}/{}). Skipping entry for file info query.", target_path.string(), error.value(), error.message());

            file_infos.emplace_back(FileInfoResponse::StatusType::FAILED_TO_QUERY_INFO, "", DirectoryEntry{});
            continue;
        }

        if(status.type() == fs::file_type::directory) {
            DirectoryEntry dentry{};
            dentry.type = DirectoryEntry::DIRECTORY;
            dentry.name = target_path.filename();
            dentry.empty = fs::is_empty(target_path, error);
            if(error)
                logWarning(LOG_FT, "Failed to query directory empty state for directory {} ({}/{})", target_path.string(), error.value(), error.message());

            dentry.modified_at = fs::last_write_time(target_path, error);
            if(error)
                logWarning(LOG_FT, "Failed to query last write time for directory {} ({}/{})", target_path.string(), error.value(), error.message());
            dentry.size = 0;
            file_infos.emplace_back(FileInfoResponse::StatusType::SUCCESS, "", dentry);
        } else if(status.type() == fs::file_type::regular) {
            DirectoryEntry dentry{};
            dentry.type = DirectoryEntry::FILE;
            dentry.name = target_path.filename();

            dentry.modified_at = fs::last_write_time(target_path, error);
            if(error)
                logWarning(LOG_FT, "Failed to query last write time for file {} ({}/{}).", target_path.string(), error.value(), error.message());
            dentry.size = fs::file_size(target_path, error);
            if(error)
                logWarning(LOG_FT, "Failed to query size for file {} ({}/{}).", target_path.string(), error.value(), error.message());
            file_infos.emplace_back(FileInfoResponse::StatusType::SUCCESS, "", dentry);
        } else {
            logWarning(LOG_FT, "File info query contains an unknown file type for file {} ({}).", target_path.string(), (int) status.type());
            file_infos.emplace_back(FileInfoResponse::StatusType::UNKNOWN_FILE_TYPE, "", DirectoryEntry{});
        }
    }

    response->emplace_success(FileInfoResponse{file_infos});
    return response;
}

std::shared_ptr<ExecuteResponse<FileInfoError, FileInfoResponse>> LocalFileSystem::query_channel_info(const std::shared_ptr<VirtualFileServer> &sid, const std::vector<std::tuple<ChannelId, std::string>>& files) {
    std::vector<std::tuple<fs::path, std::string>> file_paths{};
    for(const auto& [channelId, path] : files)
        file_paths.emplace_back(this->server_channel_path(sid, channelId), path);
    return this->query_file_info(file_paths);
}

std::shared_ptr<ExecuteResponse<FileInfoError, FileInfoResponse>> LocalFileSystem::query_icon_info(const std::shared_ptr<VirtualFileServer> &sid, const std::vector<std::string> &paths) {
    std::vector<std::tuple<fs::path, std::string>> file_paths{};
    for(const auto& path : paths)
        file_paths.emplace_back(this->server_path(sid) / fs::u8path("icons"), path);
    return this->query_file_info(file_paths);
}

std::shared_ptr<ExecuteResponse<FileInfoError, FileInfoResponse> > LocalFileSystem::query_avatar_info(const std::shared_ptr<VirtualFileServer> &sid, const std::vector<std::string> &paths) {
    std::vector<std::tuple<fs::path, std::string>> file_paths{};
    for(const auto& path : paths)
        file_paths.emplace_back(this->server_path(sid) / fs::u8path("avatars"), path);
    return this->query_file_info(file_paths);
}