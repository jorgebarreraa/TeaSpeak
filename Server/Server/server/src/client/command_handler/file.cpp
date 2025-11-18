//
// Created by wolverindev on 26.01.20.
//

#include <memory>

#include <bitset>
#include <algorithm>
#include <openssl/sha.h>
#include "../ConnectedClient.h"
#include "../InternalClient.h"
#include "../../server/VoiceServer.h"
#include "../voice/VoiceClient.h"
#include "../../InstanceHandler.h"
#include "../../server/QueryServer.h"
#include "../music/MusicClient.h"
#include "../query/QueryClient.h"
#include "../../manager/ConversationManager.h"
#include "../../manager/PermissionNameMapper.h"
#include "../../manager/ActionLogger.h"
#include "helpers.h"

#include <files/FileServer.h>

#include <misc/sassert.h>
#include <misc/base64.h>
#include <misc/hex.h>
#include <misc/rnd.h>

#include <log/LogUtils.h>

using namespace std::chrono;
using namespace std;
using namespace ts;
using namespace ts::server;

constexpr static auto kFileAPITimeout = std::chrono::milliseconds{500};
constexpr static auto kMaxClientTransfers = 10;

//ftgetfilelist cid=1 cpw path=\/ return_code=1:x
//Answer:
//1 .. n
//  notifyfilelist cid=1 path=\/ return_code=1:x name=testFile size=35256 datetime=1509459767 type=1|name=testDir size=0 datetime=1509459741 type=0|name=testDir_2 size=0 datetime=1509459763 type=0
//notifyfilelistfinished cid=1 path=\/

command_result ConnectedClient::handleCommandFTGetFileList(Command &cmd) {
    using directory_query_response_t = file::filesystem::AbstractProvider::directory_query_response_t;
    using DirectoryEntry = file::filesystem::DirectoryEntry;

    CMD_REQ_SERVER;
    CMD_RESET_IDLE;
    auto virtual_file_server = file::server()->find_virtual_server(this->getServerId());
    if(!virtual_file_server) return command_result{error::file_virtual_server_not_registered};

    auto& file_system = file::server()->file_system();

    auto directory_path = cmd["path"].string();
    std::shared_ptr<directory_query_response_t> query_result{};
    if (cmd[0].has("cid") && cmd["cid"] != 0) { //Channel
        auto channel = this->server->channelTree->findChannel(cmd["cid"].as<ChannelId>());
        if (!channel)
            return command_result{error::channel_invalid_id};

        auto channel_password = cmd["cpw"].optional_string();
        if (!channel->verify_password(channel_password, this->getType() != ClientType::CLIENT_QUERY) && !permission::v2::permission_granted(1, this->calculate_permission(permission::b_ft_ignore_password, channel->channelId()))) {
            return channel_password.has_value() ? command_result{error::channel_invalid_password} : command_result{permission::b_ft_ignore_password};
        }

        if(!channel->permission_granted(permission::i_ft_needed_file_browse_power, this->calculate_permission(permission::i_ft_file_browse_power, channel->channelId()), true))
            return command_result{permission::i_ft_file_browse_power};

        query_result = file_system.query_channel_directory(virtual_file_server, cmd["cid"].as<ChannelId>(), cmd["path"].string());
    } else {
        if (directory_path == "/icons" || directory_path == "/icons/") {
            if(!permission::v2::permission_granted(1, this->calculate_permission(permission::b_icon_manage, 0)))
                return command_result{permission::b_icon_manage};

            query_result = file_system.query_icon_directory(virtual_file_server);
        } else if (directory_path == "/") {
            if(!permission::v2::permission_granted(1, this->calculate_permission(permission::b_icon_manage, 0)))
                return command_result{permission::b_icon_manage};

            query_result = file_system.query_avatar_directory(virtual_file_server);
        } else {
            debugMessage(this->getServerId(), "{} Requested file list for unknown path/name: path: {} name: {}", cmd["path"].string(), cmd["name"].string());
            return command_result{error::parameter_invalid, "path"};
        }
    }

    if(!query_result->wait_for(kFileAPITimeout)) {
        return command_result{error::file_api_timeout};
    }

    if(!query_result->succeeded()) {
        debugMessage(this->getServerId(), "{} Failed to query directory: {} / {}", CLIENT_STR_LOG_PREFIX, file::filesystem::directory_query_error_messages[(int) query_result->error().error_type], query_result->error().error_message);
        using ErrorType = file::filesystem::DirectoryQueryErrorType;
        switch(query_result->error().error_type) {
            case ErrorType::UNKNOWN:
            case ErrorType::FAILED_TO_LIST_FILES:
                return command_result{error::vs_critical, query_result->error().error_message};

            case ErrorType::PATH_IS_A_FILE:
            case ErrorType::PATH_EXCEEDS_ROOT_PATH:
                return command_result{error::file_invalid_path};

            case ErrorType::PATH_DOES_NOT_EXISTS:
                /* directory has not been created because there are no files */
                if(directory_path.empty() || directory_path == "/")
                    return command_result{error::database_empty_result};
                return command_result{error::file_not_found};

            default:
                assert(false);
                return command_result{error::vs_critical};
        }
    }

    const auto& files = query_result->response();
    if(files.empty()) {
        return command_result{error::database_empty_result};
    }

    auto return_code = cmd["return_code"].size() > 0 ? cmd["return_code"].string() : "";

    {
        ts::command_builder notify_file_list{this->notify_response_command("notifyfilelist")};
        size_t bulk_index{0};
        for(const auto& file : files) {
            if(bulk_index == 0) {
                notify_file_list.reset();
                notify_file_list.put_unchecked(0, "path", cmd["path"].string());
                notify_file_list.put_unchecked(0, "cid", cmd["cid"].string());
                if(!return_code.empty()){
                    notify_file_list.put_unchecked(0, "return_code", return_code);
                }
            }
            auto bulk = notify_file_list.bulk(bulk_index++);

            switch(file.type) {
                case DirectoryEntry::DIRECTORY:
                    bulk.put_unchecked("type", "0");
                    break;

                case DirectoryEntry::FILE:
                    bulk.put_unchecked("type", "1");
                    break;

                case DirectoryEntry::UNKNOWN:
                    bulk.reset();
                    bulk_index--;
                    continue;
            }

            bulk.put_unchecked("name", file.name);
            bulk.put_unchecked("size", file.size);
            bulk.put_unchecked("empty", file.empty);
            bulk.put_unchecked("datetime", std::chrono::duration_cast<std::chrono::seconds>(file.modified_at.time_since_epoch()).count());
            if(bulk_index >= 16 && this->getType() != ClientType::CLIENT_QUERY) {
                this->sendCommand(notify_file_list);
                bulk_index = 0;
            }
        }
        if(bulk_index > 0)
            this->sendCommand(notify_file_list);
    }

    if(this->getExternalType() != ClientType::CLIENT_QUERY) {
        ts::command_builder notify_file_list_finished{this->notify_response_command("notifyfilelistfinished")};
        notify_file_list_finished.put_unchecked(0, "path", cmd["path"].string());
        notify_file_list_finished.put_unchecked(0, "cid", cmd["cid"].string());
        if(!return_code.empty())
            notify_file_list_finished.put_unchecked(0, "return_code", return_code);
        this->sendCommand(notify_file_list_finished);
    }
    return command_result{error::ok};
}

//ftcreatedir cid=4 cpw dirname=\/TestDir return_code=1:17
command_result ConnectedClient::handleCommandFTCreateDir(Command &cmd) {
    using ErrorType = file::filesystem::DirectoryModifyErrorType;

    CMD_REQ_SERVER;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    auto virtual_file_server = file::server()->find_virtual_server(this->getServerId());
    if(!virtual_file_server) return command_result{error::file_virtual_server_not_registered};
    auto& file_system = file::server()->file_system();

    std::shared_lock channel_tree_lock{this->server->channel_tree_mutex};
    auto channel = this->server->channelTree->findChannel(cmd["cid"].as<ChannelId>());
    if (!channel)
        return command_result{error::channel_invalid_id};

    auto channel_password = cmd["cpw"].optional_string();
    if (!channel->verify_password(channel_password, this->getType() != ClientType::CLIENT_QUERY) && !permission::v2::permission_granted(1, this->calculate_permission(permission::b_ft_ignore_password, channel->channelId()))) {
        return channel_password.has_value() ? command_result{error::channel_invalid_password} : command_result{permission::b_ft_ignore_password};
    }

    if(!channel->permission_granted(permission::i_ft_needed_directory_create_power, this->calculate_permission(permission::i_ft_directory_create_power, channel->channelId()), true))
        return command_result{permission::i_ft_directory_create_power};
    channel_tree_lock.unlock();

    auto create_result = file_system.create_channel_directory(virtual_file_server, channel->channelId(), cmd["dirname"].string());
    if(!create_result->wait_for(kFileAPITimeout))
        return command_result{error::file_api_timeout};

    if(!create_result->succeeded()) {
        debugMessage(this->getServerId(), "{} Failed to create channel directory: {} / {}", CLIENT_STR_LOG_PREFIX, (int) create_result->error().error_type, create_result->error().error_message);
        switch(create_result->error().error_type) {
            case ErrorType::UNKNOWN:
            case ErrorType::FAILED_TO_CREATE_DIRECTORIES: {
                auto error_detail = std::to_string((int) create_result->error().error_type);
                if(!create_result->error().error_message.empty())
                    error_detail += "/" + create_result->error().error_message;
                return command_result{error::file_io_error, error_detail};
            }
            case ErrorType::PATH_ALREADY_EXISTS:
                return command_result{error::file_already_exists};

            case ErrorType::PATH_EXCEEDS_ROOT_PATH:
                return command_result{error::file_invalid_path};
        }
    }

    serverInstance->action_logger()->file_logger.log_file_directory_create(this->getServerId(), this->ref(), channel->channelId(), cmd["dirname"].string());

    return command_result{error::ok};
}


command_result ConnectedClient::handleCommandFTDeleteFile(Command &cmd) {
    using ErrorType = file::filesystem::FileDeleteErrorType;

    CMD_REQ_SERVER;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    auto virtual_file_server = file::server()->find_virtual_server(this->getServerId());
    if(!virtual_file_server) return command_result{error::file_virtual_server_not_registered};
    auto& file_system = file::server()->file_system();

    ts::command_result_bulk response{};
    response.emplace_result_n(cmd.bulkCount(), error::ok);

    std::vector<std::tuple<uint64_t, std::string>> file_log_info{};

    auto file_path = cmd["path"].string();
    std::shared_ptr<file::ExecuteResponse<file::filesystem::FileDeleteError, file::filesystem::FileDeleteResponse>> delete_response{};
    if (cmd[0].has("cid") && cmd["cid"] != 0) {
        std::shared_lock channel_tree_lock{this->server->channel_tree_mutex};
        auto channel = this->server->channelTree->findChannel(cmd["cid"].as<ChannelId>());
        if (!channel)
            return command_result{error::channel_invalid_id};

        auto channel_password = cmd["cpw"].optional_string();
        if (!channel->verify_password(channel_password, this->getType() != ClientType::CLIENT_QUERY) && !permission::v2::permission_granted(1, this->calculate_permission(permission::b_ft_ignore_password, channel->channelId()))) {
            return channel_password.has_value() ? command_result{error::channel_invalid_password} : command_result{permission::b_ft_ignore_password};
        }

        if(!channel->permission_granted(permission::i_ft_needed_file_delete_power, this->calculate_permission(permission::i_ft_file_delete_power, channel->channelId()), true))
            return command_result{permission::i_ft_file_delete_power};

        std::vector<std::string> delete_files{};
        delete_files.reserve(cmd.bulkCount());
        file_log_info.reserve(cmd.bulkCount());
        for(size_t index{0}; index < cmd.bulkCount(); index++) {
            delete_files.push_back(file_path + "/" + cmd[index]["name"].string());
            file_log_info.emplace_back(channel->channelId(), file_path + "/" + cmd[index]["name"].string());
        }

        delete_response = file_system.delete_channel_files(virtual_file_server, channel->channelId(), delete_files);
    } else {
        auto first_entry_name = cmd["name"].string();
        if (first_entry_name.find("/icon_") == 0 && file_path.empty()) {
            if(!permission::v2::permission_granted(1, this->calculate_permission(permission::b_icon_manage, 0)))
                return command_result{permission::b_icon_manage};

            std::vector<std::string> delete_files{};
            delete_files.reserve(cmd.bulkCount());
            file_log_info.reserve(cmd.bulkCount());
            for(size_t index{0}; index < cmd.bulkCount(); index++) {
                auto file_name = cmd[index]["name"].string();
                if(!file_name.starts_with("/icon_")) {
                    response.set_result(index, ts::command_result{error::parameter_constraint_violation});
                    continue;
                }

                delete_files.push_back(file_name);
                file_log_info.emplace_back(0, file_name);
            }

            delete_response = file_system.delete_icons(virtual_file_server, delete_files);
        } else if (first_entry_name.starts_with("/avatar_") && file_path.empty()) {
            enum PermissionTestState {
                SUCCEEDED,
                FAILED,
                UNSET
            } permission_delete_other{PermissionTestState::UNSET};

            std::vector<std::string> delete_files{};
            delete_files.reserve(cmd.bulkCount());
            file_log_info.reserve(cmd.bulkCount());
            for(size_t index{0}; index < cmd.bulkCount(); index++) {
                auto file_name = cmd[index]["name"].string();
                if(!file_name.starts_with("/avatar_")) {
                    response.set_result(index, ts::command_result{error::parameter_constraint_violation});
                    continue;
                }

                if (file_name != "/avatar_") {
                    if(permission_delete_other == PermissionTestState::UNSET) {
                        if(permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_avatar_delete_other, 0)))
                            permission_delete_other = PermissionTestState::SUCCEEDED;
                        else
                            permission_delete_other = PermissionTestState::FAILED;
                    }

                    if(permission_delete_other != PermissionTestState::SUCCEEDED) {
                        response.set_result(index, ts::command_result{permission::b_client_avatar_delete_other});
                        continue;
                    }

                    auto uid = file_name.substr(strlen("/avatar_"));
                    auto avId = hex::hex(base64::decode(uid), 'a', 'q');

                    auto cls = this->server->findClientsByUid(uid);
                    for (const auto &cl : cls) {
                        cl->properties()[property::CLIENT_FLAG_AVATAR] = "";
                        this->server->notifyClientPropertyUpdates(cl, deque<property::ClientProperties>{property::CLIENT_FLAG_AVATAR});
                    }

                    delete_files.push_back("/avatar_" + avId);
                    file_log_info.emplace_back(0, "/avatar_" + avId);
                } else {
                    this->properties()[property::CLIENT_FLAG_AVATAR] = "";
                    this->server->notifyClientPropertyUpdates(this->ref(), deque<property::ClientProperties>{property::CLIENT_FLAG_AVATAR});
                    delete_files.push_back("/avatar_" + this->getAvatarId());
                    file_log_info.emplace_back(0, "/avatar_" + this->getAvatarId());
                }
            }

            delete_response = file_system.delete_avatars(virtual_file_server, delete_files);
        } else {
            logError(this->getServerId(), "Unknown requested file to delete: {}", cmd["path"].as<std::string>());
            return command_result{error::not_implemented};
        }
    }

    if(!delete_response->wait_for(kFileAPITimeout))
        return command_result{error::file_api_timeout};

    if(!delete_response->succeeded()) {
        debugMessage(this->getServerId(), "{} Failed to create channel directory: {} / {}", CLIENT_STR_LOG_PREFIX, (int) delete_response->error().error_type, delete_response->error().error_message);
        switch(delete_response->error().error_type) {
            case ErrorType::UNKNOWN:  {
                auto error_detail = std::to_string((int) delete_response->error().error_type);
                if(!delete_response->error().error_message.empty())
                    error_detail += "/" + delete_response->error().error_message;
                return command_result{error::vs_critical, error_detail};
            }
        }
    }

    const auto& file_status = delete_response->response();
    size_t bulk_index{0};
    for(size_t index{0}; index < file_status.delete_results.size(); index++) {
        const auto& file = file_status.delete_results[index];
        const auto& log_file_info = file_log_info[index];

        while(response.response(bulk_index).error_code() != error::ok)
            bulk_index++;

        using Status = file::filesystem::FileDeleteResponse::StatusType;
        switch (file.status) {
            case Status::SUCCESS:
                serverInstance->action_logger()->file_logger.log_file_delete(this->getServerId(), this->ref(), std::get<0>(log_file_info), std::get<1>(log_file_info));
                /* we already emplaced success */
                break;

            case Status::PATH_EXCEEDS_ROOT_PATH:
                response.set_result(bulk_index, ts::command_result{error::file_invalid_path});
                break;

            case Status::PATH_DOES_NOT_EXISTS:
                response.set_result(bulk_index, ts::command_result{error::file_not_found});
                break;

            case Status::SOME_FILES_ARE_LOCKED:
                response.set_result(bulk_index, ts::command_result{error::file_already_in_use, file.error_detail});
                break;

            case Status::FAILED_TO_DELETE_FILES:
                response.set_result(bulk_index, ts::command_result{error::file_io_error, file.error_detail});
                break;
        }
        bulk_index++;
    }

    while(response.length() > bulk_index && response.response(bulk_index).type() == command_result_type::error && response.response(bulk_index).error_code() != error::ok)
        bulk_index++;
    assert(bulk_index == cmd.bulkCount());
    return command_result{std::move(response)};
}


/*
 * Usage: ftgetfileinfo cid={channelID} cpw={channelPassword} name={filePath}...

Permissions:
  i_ft_file_browse_power
  i_ft_needed_file_browse_power

Description:
  Displays detailed information about one or more specified files stored in a
  channels file repository.

Example:
  ftgetfileinfo cid=2 cpw= path=\/Pic1.PNG|cid=2 cpw= path=\/Pic2.PNG
  cid=2 path=\/ name=Stuff size=0 datetime=1259415210 type=0|name=Pic1.PNG size=563783 datetime=1259425462 type=1|name=Pic2.PNG ...
  error id=0 msg=ok

 */

command_result ConnectedClient::handleCommandFTGetFileInfo(ts::Command &cmd) {
    using ErrorType = file::filesystem::FileInfoErrorType;
    using DirectoryEntry = file::filesystem::DirectoryEntry;

    CMD_RESET_IDLE;
    CMD_REQ_SERVER;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    auto virtual_file_server = file::server()->find_virtual_server(this->getServerId());
    if(!virtual_file_server) return command_result{error::file_virtual_server_not_registered};
    auto& file_system = file::server()->file_system();

    ts::command_result_bulk response{};
    response.emplace_result_n(cmd.bulkCount(), error::ok);

    auto file_path = cmd["path"].string();
    std::shared_ptr<file::ExecuteResponse<file::filesystem::FileInfoError, file::filesystem::FileInfoResponse>> info_response{};
    if (cmd[0].has("cid") && cmd["cid"] != 0) {
        std::shared_ptr<BasicChannel> currentChannel{};
        error::type error_error{error::ok};
        permission::PermissionType permission_error{permission::ok};

        std::vector<std::tuple<ChannelId, std::string>> info_files{};
        info_files.reserve(cmd.bulkCount());

        for(size_t index{0}; index < cmd.bulkCount(); index++) {
            if(cmd[index].has("cid")) {
                error_error = error::ok;
                permission_error = permission::ok;

                currentChannel = this->server->channelTree->findChannel(cmd[index]["cid"].as<ChannelId>());
                if(currentChannel) {
                    const auto password = cmd[index]["cpw"].string();
                    if (!currentChannel->verify_password(std::make_optional(password), this->getType() != ClientType::CLIENT_QUERY) && !permission::v2::permission_granted(1, this->calculate_permission(permission::b_ft_ignore_password, currentChannel->channelId()))) {
                        if(password.empty())
                            error_error = error::channel_invalid_password;
                        else
                            permission_error = permission::b_ft_ignore_password;
                    } else if(!currentChannel->permission_granted(permission::i_ft_needed_file_browse_power, this->calculate_permission(permission::i_ft_file_browse_power, currentChannel->channelId()), true)) {
                        permission_error = permission::i_ft_file_browse_power;
                    }
                } else {
                    error_error = error::channel_invalid_id;
                }
            }

            if(error_error != error::ok) {
                response.set_result(index, ts::command_result{error_error});
                continue;
            }

            if(permission_error != permission::ok) {
                response.set_result(index, ts::command_result{permission_error});
                continue;
            }
            info_files.emplace_back(currentChannel->channelId(), cmd[index]["name"].string());
        }

        info_response = file_system.query_channel_info(virtual_file_server, info_files);
    } else {
        auto first_entry_name = cmd["name"].string();
        if (first_entry_name.find("/icon_") == 0 && file_path.empty()) {
            if(!permission::v2::permission_granted(1, this->calculate_permission(permission::b_icon_manage, 0)))
                return command_result{permission::b_icon_manage};

            std::vector<std::string> delete_files{};
            delete_files.reserve(cmd.bulkCount());
            for(size_t index{0}; index < cmd.bulkCount(); index++) {
                auto file_name = cmd[index]["name"].string();
                if(!file_name.starts_with("/icon_")) {
                    response.set_result(index, ts::command_result{error::parameter_constraint_violation});
                    continue;
                }

                delete_files.push_back(file_name);
            }

            info_response = file_system.query_icon_info(virtual_file_server, delete_files);
        } else if (first_entry_name.starts_with("/avatar_") && file_path.empty()) {
            if(!permission::v2::permission_granted(1, this->calculate_permission(permission::b_icon_manage, 0)))
                return command_result{permission::b_icon_manage};

            std::vector<std::string> delete_files{};
            delete_files.reserve(cmd.bulkCount());
            for(size_t index{0}; index < cmd.bulkCount(); index++) {
                auto file_name = cmd[index]["name"].string();
                if(!file_name.starts_with("/avatar_")) {
                    response.set_result(index, ts::command_result{error::parameter_constraint_violation});
                    continue;
                }

                if (file_name != "/avatar_") {
                    auto uid = file_name.substr(strlen("/avatar_"));
                    auto avId = hex::hex(base64::decode(uid), 'a', 'q');
                    delete_files.push_back("/avatar_" + avId);
                } else {
                    delete_files.push_back("/avatar_" + this->getAvatarId());
                }
            }

            info_response = file_system.query_avatar_info(virtual_file_server, delete_files);
        } else {
            logError(this->getServerId(), "Unknown requested file to query info: {}", cmd["path"].as<std::string>());
            return command_result{error::parameter_invalid};
        }
    }

    if(!info_response->wait_for(kFileAPITimeout)) {
        return command_result{error::file_api_timeout};
    }

    if(!info_response->succeeded()) {
        debugMessage(this->getServerId(), "{} Failed to execute file info query: {} / {}", CLIENT_STR_LOG_PREFIX, (int) info_response->error().error_type, info_response->error().error_message);
        switch(info_response->error().error_type) {
            case ErrorType::UNKNOWN: {
                auto error_detail = std::to_string((int) info_response->error().error_type);
                if(!info_response->error().error_message.empty()) {
                    error_detail += "/" + info_response->error().error_message;
                }
                return command_result{error::vs_critical, error_detail};
            }
        }
    }

    const auto as_list = cmd.hasParm("as-list");
    const auto& file_status = info_response->response();
    size_t bulk_index{0};

    ts::command_builder notify_file_info{this->notify_response_command("notifyfileinfo")};
    size_t notify_index{0};
    for(const auto& file : file_status.file_info) {
        while(response.response(bulk_index).error_code() != error::ok) {
            bulk_index++;
        }

        using Status = file::filesystem::FileInfoResponse::StatusType;
        switch (file.status) {
            case Status::SUCCESS:
                /* we already emplaced success */
                break;

            case Status::PATH_EXCEEDS_ROOT_PATH:
                response.set_result(bulk_index, ts::command_result{error::file_invalid_path});
                break;

            case Status::PATH_DOES_NOT_EXISTS:
            case Status::UNKNOWN_FILE_TYPE:
                response.set_result(bulk_index, ts::command_result{error::file_not_found});
                break;

            case Status::FAILED_TO_QUERY_INFO:
                response.set_result(bulk_index, ts::command_result{error::file_io_error, file.error_detail});
                break;
        }
        bulk_index++;
        if(notify_index == 0) {
            notify_file_info.reset();
            notify_file_info.put_unchecked(0, "return_code", cmd["return_code"].string());
        }

        auto bulk = notify_file_info.bulk(notify_index++);
        switch(file.info.type) {
            case DirectoryEntry::DIRECTORY:
                bulk.put_unchecked("type", "0");
                break;

            case DirectoryEntry::FILE:
                bulk.put_unchecked("type", "1");
                break;

            case DirectoryEntry::UNKNOWN:
                bulk.reset();
                notify_index--;
                continue;
        }

        bulk.put_unchecked("name", file.info.name);
        bulk.put_unchecked("size", file.info.size);
        bulk.put_unchecked("empty", file.info.empty);
        bulk.put_unchecked("datetime", std::chrono::duration_cast<std::chrono::seconds>(file.info.modified_at.time_since_epoch()).count());

        if(notify_index > 20 && as_list) {
            this->sendCommand(notify_file_info);
            notify_index = 0;
        }
    }
    if(notify_index > 0) {
        this->sendCommand(notify_file_info);
    }

    if(as_list && this->getExternalType() == ClientType::CLIENT_TEAMSPEAK) {
        ts::command_builder notify{this->notify_response_command("notifyfileinfofinished")};
        notify.put_unchecked(0, "return_code", cmd["return_code"].string());
        this->sendCommand(notify);
    }

    while(response.length() > bulk_index && response.response(bulk_index).type() == command_result_type::error && response.response(bulk_index).error_code() != error::ok) {
        bulk_index++;
    }

    assert(bulk_index == cmd.bulkCount());
    return command_result{std::move(response)};
}

/*
ftinitupload clientftfid={clientFileTransferID} name={filePath}
       cid={channelID} cpw={channelPassword} size={fileSize} overwrite={1|0}
       resume={1|0} [proto=0-1]
 */
command_result ConnectedClient::handleCommandFTInitUpload(ts::Command &cmd) {
    CMD_REQ_SERVER;

    auto virtual_file_server = file::server()->find_virtual_server(this->getServerId());
    if(!virtual_file_server) return command_result{error::file_virtual_server_not_registered};

    if(!cmd[0].has("path")) cmd["path"] = "";

    file::transfer::AbstractProvider::TransferInfo info{};
    std::shared_ptr<file::ExecuteResponse<file::transfer::TransferInitError, std::shared_ptr<file::transfer::Transfer>>> transfer_response{};

    info.max_bandwidth = -1;
    {
        auto max_quota = this->calculate_permission(permission::i_ft_max_bandwidth_upload, this->getClientId());
        if(max_quota.has_value)
            info.max_bandwidth = max_quota.value;
    }
    info.file_offset = 0;
    info.expected_file_size = cmd["size"].as<size_t>();
    info.override_exiting = cmd["overwrite"].as<bool>();
    info.file_path = cmd["path"].string() + "/" + cmd["name"].string();
    info.client_unique_id = this->getUid();
    info.client_id = this->getClientId();
    info.max_concurrent_transfers = kMaxClientTransfers;

    /* TODO: Save last file offset and resume */
    if(cmd["resume"].as<bool>() && info.override_exiting)
        return ts::command_result{error::file_overwrite_excludes_resume};

    {
        auto server_quota = this->server->properties()[property::VIRTUALSERVER_UPLOAD_QUOTA].as_unchecked<ssize_t>();
        auto server_used_quota = this->server->properties()[property::VIRTUALSERVER_MONTH_BYTES_UPLOADED].as_unchecked<size_t>();
        server_used_quota += cmd["size"].as<uint64_t>();
        if(server_quota >= 0 && server_quota * 1024 * 1024 < (int64_t) server_used_quota) return command_result{error::file_transfer_server_quota_exceeded};

        auto client_quota = this->calculate_permission(permission::i_ft_quota_mb_upload_per_client, 0);
        auto client_used_quota = this->properties()[property::CLIENT_MONTH_BYTES_UPLOADED].as_unchecked<size_t>();
        client_used_quota += cmd["size"].as<uint64_t>();
        if(client_quota.has_value && !client_quota.has_infinite_power() && (client_quota.value < 0 || client_quota.value * 1024 * 1024 < (int64_t) client_used_quota))
            return command_result{error::file_transfer_client_quota_exceeded};
    }

    ChannelId log_channel_id{0};
    if(cmd[0].has("cid") && cmd["cid"] != 0) { //Channel
        std::shared_lock channel_tree_lock{this->server->channel_tree_mutex};
        auto channel = this->server->channelTree->findChannel(cmd["cid"].as<ChannelId>());
        if (!channel)
            return command_result{error::channel_invalid_id, "Cant resolve channel"};

        auto channel_password = cmd["cpw"].optional_string();
        if (!channel->verify_password(channel_password, this->getType() != ClientType::CLIENT_QUERY) && !permission::v2::permission_granted(1, this->calculate_permission(permission::b_ft_ignore_password, channel->channelId()))) {
            return channel_password.has_value() ? command_result{error::channel_invalid_password} : command_result{permission::b_ft_ignore_password};
        }

        ACTION_REQUIRES_CHANNEL_PERMISSION(channel, permission::i_ft_needed_file_upload_power, permission::i_ft_file_upload_power, true);
        transfer_response = file::server()->file_transfer().initialize_channel_transfer(file::transfer::Transfer::DIRECTION_UPLOAD, virtual_file_server, channel->channelId(), info);
        log_channel_id = channel->channelId();
    } else {
        if (cmd["path"].string().empty() && cmd["name"].string().starts_with("/icon_")) {
            auto max_size = this->calculate_permission(permission::i_max_icon_filesize, 0);
            if(max_size.has_value && !max_size.has_infinite_power() && (max_size.value < 0 || max_size.value < cmd["size"].as<size_t>()))
                return command_result{permission::i_max_icon_filesize};

            transfer_response = file::server()->file_transfer().initialize_icon_transfer(file::transfer::Transfer::DIRECTION_UPLOAD, virtual_file_server, info);
        } else if (cmd["path"].as<std::string>().empty() && cmd["name"].string() == "/avatar") {
            auto max_size = this->calculate_permission(permission::i_client_max_avatar_filesize, 0);
            if(max_size.has_value && !max_size.has_infinite_power() && (max_size.value < 0 || max_size.value < cmd["size"].as<size_t>()))
                return command_result{permission::i_client_max_avatar_filesize};

            info.file_path = "/avatar_" + this->getAvatarId();
            transfer_response = file::server()->file_transfer().initialize_avatar_transfer(file::transfer::Transfer::DIRECTION_UPLOAD, virtual_file_server, info);
        } else {
            return command_result{error::parameter_invalid, "name"};
        }
    }

    if(!transfer_response->wait_for(kFileAPITimeout))
        return command_result{error::file_api_timeout};

    if(!transfer_response->succeeded()) {
        using ErrorType = file::transfer::TransferInitError;

        debugMessage(this->getServerId(), "{} Failed to initialize file upload: {} / {}", CLIENT_STR_LOG_PREFIX, (int) transfer_response->error().error_type, transfer_response->error().error_message);
        switch(transfer_response->error().error_type) {
            case ErrorType::UNKNOWN: {
                auto error_detail = std::to_string((int) transfer_response->error().error_type);
                if(!transfer_response->error().error_message.empty())
                    error_detail += "/" + transfer_response->error().error_message;
                return command_result{error::vs_critical, error_detail};
            }
            case ErrorType::IO_ERROR:
                return command_result{error::file_io_error, transfer_response->error().error_message};

            case ErrorType::FILE_IS_NOT_A_FILE:
            case ErrorType::INVALID_FILE_TYPE:
            case ErrorType::FILE_DOES_NOT_EXISTS:
                return command_result{error::file_not_found};

            case ErrorType::SERVER_QUOTA_EXCEEDED:
                return command_result{error::file_transfer_server_quota_exceeded};

            case ErrorType::CLIENT_QUOTA_EXCEEDED:
                return command_result{error::file_transfer_client_quota_exceeded};

            case ErrorType::SERVER_TOO_MANY_TRANSFERS:
                return command_result{error::file_server_transfer_limit_reached};

            case ErrorType::CLIENT_TOO_MANY_TRANSFERS:
                return command_result{error::file_client_transfer_limit_reached};
        }
    }

    auto transfer = transfer_response->response();
    if(transfer->server_addresses.empty()) {
        logError(0, "{} Received transfer without any addresses!", CLIENT_STR_LOG_PREFIX);
        return ts::command_result{error::vs_critical};
    }
    transfer->client_transfer_id = cmd["clientftfid"];

    ts::command_builder result{this->notify_response_command("notifystartupload")};
    result.put_unchecked(0, "clientftfid", cmd["clientftfid"].string());
    result.put_unchecked(0, "serverftfid", transfer->server_transfer_id);

    auto used_address = transfer->server_addresses[0];
    result.put_unchecked(0, "ip", used_address.hostname);
    result.put_unchecked(0, "port", used_address.port);

    result.put_unchecked(0, "ftkey", transfer->transfer_key);
    result.put_unchecked(0, "seekpos", transfer->file_offset);
    result.put_unchecked(0, "proto", "1");
    this->sendCommand(result);

    serverInstance->action_logger()->file_logger.log_file_upload(this->getServerId(), this->ref(), log_channel_id, info.file_path);
    return command_result{error::ok};
}

/*
ftinitdownload clientftfid={clientFileTransferID} name={filePath}
               cid={channelID} cpw={channelPassword} seekpos={seekPosition} [proto=0-1]
 */
command_result ConnectedClient::handleCommandFTInitDownload(ts::Command &cmd) {
    CMD_REQ_SERVER;

    auto virtual_file_server = file::server()->find_virtual_server(this->getServerId());
    if(!virtual_file_server) return command_result{error::file_virtual_server_not_registered};
    if(!cmd[0].has("path")) cmd["path"] = "";

    file::transfer::AbstractProvider::TransferInfo info{};
    std::shared_ptr<file::ExecuteResponse<file::transfer::TransferInitError, std::shared_ptr<file::transfer::Transfer>>> transfer_response{};

    {
        auto server_quota = this->server->properties()[property::VIRTUALSERVER_DOWNLOAD_QUOTA].as_unchecked<ssize_t>();
        auto server_used_quota = this->server->properties()[property::VIRTUALSERVER_MONTH_BYTES_DOWNLOADED].as_unchecked<size_t>();
        if(server_quota >= 0) {
            if((size_t) server_quota * 1024 * 1024 <= server_used_quota)
                return command_result{error::file_transfer_server_quota_exceeded};
            info.download_server_quota_limit = server_quota * 1024 * 1024 - server_used_quota;
        }


        auto client_quota = this->calculate_permission(permission::i_ft_quota_mb_download_per_client, 0);
        auto client_used_quota = this->properties()[property::CLIENT_MONTH_BYTES_DOWNLOADED].as_unchecked<size_t>();
        if(client_quota.has_value) {
            if(client_quota.value > 0) {
                if((size_t) client_quota.value * 1024 * 1024 <= client_used_quota)
                    return command_result{error::file_transfer_client_quota_exceeded};
                info.download_client_quota_limit = client_quota.value * 1024 * 1024 - client_used_quota;
            } else if(client_quota.value != -1) {
                return command_result{error::file_transfer_client_quota_exceeded};
            }
        }
    }

    info.max_bandwidth = -1;
    {
        auto max_quota = this->calculate_permission(permission::i_ft_max_bandwidth_download, this->getClientId());
        if(max_quota.has_value)
            info.max_bandwidth = max_quota.value;
    }

    info.file_offset = cmd["seekpos"].as<size_t>();
    info.override_exiting = false;
    info.file_path = cmd["path"].string() + "/" + cmd["name"].string();
    info.client_unique_id = this->getUid();
    info.client_id = this->getClientId();
    info.max_concurrent_transfers = kMaxClientTransfers;

    ChannelId log_channel_id{0};
    if(cmd[0].has("cid") && cmd["cid"] != 0) { //Channel
        std::shared_lock channel_tree_lock{this->server->channel_tree_mutex};
        auto channel = this->server->channelTree->findChannel(cmd["cid"].as<ChannelId>());
        if (!channel)
            return command_result{error::channel_invalid_id};

        auto channel_password = cmd["cpw"].optional_string();
        if (!channel->verify_password(channel_password, this->getType() != ClientType::CLIENT_QUERY) && !permission::v2::permission_granted(1, this->calculate_permission(permission::b_ft_ignore_password, channel->channelId()))) {
            return channel_password.has_value() ? command_result{error::channel_invalid_password} : command_result{permission::b_ft_ignore_password};
        }

        ACTION_REQUIRES_CHANNEL_PERMISSION(channel, permission::i_ft_needed_file_download_power, permission::i_ft_file_download_power, true);
        transfer_response = file::server()->file_transfer().initialize_channel_transfer(file::transfer::Transfer::DIRECTION_DOWNLOAD, virtual_file_server, channel->channelId(), info);
        log_channel_id = channel->channelId();
    } else {
        if (cmd["path"].as<std::string>().empty() && cmd["name"].string().starts_with("/icon_")) {
            transfer_response = file::server()->file_transfer().initialize_icon_transfer(file::transfer::Transfer::DIRECTION_DOWNLOAD, virtual_file_server, info);
        } else if (cmd["path"].as<std::string>().empty() && cmd["name"].string().starts_with("/avatar")) {
            transfer_response = file::server()->file_transfer().initialize_avatar_transfer(file::transfer::Transfer::DIRECTION_DOWNLOAD, virtual_file_server, info);
        } else {
            return command_result{error::parameter_invalid, "name"};
        }
    }

    if(!transfer_response->wait_for(kFileAPITimeout))
        return command_result{error::file_api_timeout};

    if(!transfer_response->succeeded()) {
        using ErrorType = file::transfer::TransferInitError;

        debugMessage(this->getServerId(), "{} Failed to initialize file download: {}/{}", CLIENT_STR_LOG_PREFIX, (int) transfer_response->error().error_type, transfer_response->error().error_message);
        switch(transfer_response->error().error_type) {
            case ErrorType::UNKNOWN: {
                auto error_detail = std::to_string((int) transfer_response->error().error_type);
                if(!transfer_response->error().error_message.empty())
                    error_detail += "/" + transfer_response->error().error_message;
                return command_result{error::vs_critical, error_detail};
            }
            case ErrorType::IO_ERROR:
                return command_result{error::file_io_error, transfer_response->error().error_message};

            case ErrorType::FILE_IS_NOT_A_FILE:
            case ErrorType::INVALID_FILE_TYPE:
            case ErrorType::FILE_DOES_NOT_EXISTS:
                return command_result{error::file_not_found};

            case ErrorType::SERVER_QUOTA_EXCEEDED:
                return command_result{error::file_transfer_server_quota_exceeded};

            case ErrorType::CLIENT_QUOTA_EXCEEDED:
                return command_result{error::file_transfer_client_quota_exceeded};

            case ErrorType::SERVER_TOO_MANY_TRANSFERS:
                return command_result{error::file_server_transfer_limit_reached};

            case ErrorType::CLIENT_TOO_MANY_TRANSFERS:
                return command_result{error::file_client_transfer_limit_reached};
        }
    }

    auto transfer = transfer_response->response();
    if(transfer->server_addresses.empty()) {
        logError(0, "{} Received transfer without any addresses!", CLIENT_STR_LOG_PREFIX);
        return ts::command_result{error::vs_critical};
    }
    transfer->client_transfer_id = cmd["clientftfid"];

    ts::command_builder result{this->notify_response_command("notifystartdownload")};
    result.put_unchecked(0, "clientftfid", cmd["clientftfid"].string());
    result.put_unchecked(0, "serverftfid", transfer->server_transfer_id);

    auto used_address = transfer->server_addresses[0];
    result.put_unchecked(0, "ip", used_address.hostname);
    result.put_unchecked(0, "port", used_address.port);

    result.put_unchecked(0, "ftkey", transfer->transfer_key);
    result.put_unchecked(0, "proto", "1");
    result.put_unchecked(0, "size", transfer->expected_file_size);

    result.put_unchecked(0, "seekpos", transfer->file_offset);
    this->sendCommand(result);

    serverInstance->action_logger()->file_logger.log_file_download(this->getServerId(), this->ref(), log_channel_id, info.file_path);
    return command_result{error::ok};
}

/*
 * Usage: ftrenamefile cid={channelID} cpw={channelPassword}
       [tcid={targetChannelID}] [tcpw={targetChannelPassword}]
       oldname={oldFilePath} newname={newFilePath}

  i_ft_file_rename_power
  i_ft_needed_file_rename_power
 */
command_result ConnectedClient::handleCommandFTRenameFile(ts::Command &cmd) {
    CMD_RESET_IDLE;
    CMD_REQ_SERVER;
    CMD_CHK_AND_INC_FLOOD_POINTS(5);

    auto virtual_file_server = file::server()->find_virtual_server(this->getServerId());
    if(!virtual_file_server) return command_result{error::file_virtual_server_not_registered};

    auto channel_id = cmd["cid"].as<ChannelId>();
    auto target_channel_id = cmd[0].has("tcid") ? cmd["tcid"].as<ChannelId>() : channel_id;

    std::shared_lock channel_tree_lock{this->server->channel_tree_mutex};
    auto channel = this->server->channelTree->findChannel(channel_id);
    if (!channel)
        return command_result{error::channel_invalid_id};

    auto channel_password = cmd["cpw"].optional_string();
    if (!channel->verify_password(channel_password, this->getType() != ClientType::CLIENT_QUERY) && !permission::v2::permission_granted(1, this->calculate_permission(permission::b_ft_ignore_password, channel->channelId()))) {
        return channel_password.has_value() ? command_result{error::channel_invalid_password} : command_result{permission::b_ft_ignore_password};
    }

    ACTION_REQUIRES_CHANNEL_PERMISSION(channel, permission::i_ft_needed_file_rename_power, permission::i_ft_file_rename_power, true);

    if(target_channel_id != channel_id) {
        auto targetChannel = this->server->channelTree->findChannel(target_channel_id);
        if (!targetChannel)
            return command_result{error::channel_invalid_id};

        auto channel_password = cmd["cpw"].optional_string();
        if (!channel->verify_password(channel_password, this->getType() != ClientType::CLIENT_QUERY) && !permission::v2::permission_granted(1, this->calculate_permission(permission::b_ft_ignore_password, channel->channelId()))) {
            return channel_password.has_value() ? command_result{error::channel_invalid_password} : command_result{permission::b_ft_ignore_password};
        }

        ACTION_REQUIRES_CHANNEL_PERMISSION(targetChannel, permission::i_ft_needed_file_rename_power, permission::i_ft_file_rename_power, true);
    }
    channel_tree_lock.unlock();


    auto rename_response = file::server()->file_system().rename_channel_file(virtual_file_server, channel_id, cmd["oldname"].string(), target_channel_id, cmd["newname"].string());
    if(!rename_response->wait_for(kFileAPITimeout))
        return command_result{error::file_api_timeout};

    if(!rename_response->succeeded()) {
        using ErrorType = file::filesystem::FileModifyErrorType;

        debugMessage(this->getServerId(), "{} Failed to rename file: {} / {}", CLIENT_STR_LOG_PREFIX, (int) rename_response->error().error_type, rename_response->error().error_message);
        switch(rename_response->error().error_type) {
            case ErrorType::UNKNOWN:
            case ErrorType::FAILED_TO_RENAME_FILE:
            case ErrorType::FAILED_TO_DELETE_FILES:
            case ErrorType::FAILED_TO_CREATE_DIRECTORIES: {
                auto error_detail = std::to_string((int) rename_response->error().error_type);
                if(!rename_response->error().error_message.empty())
                    error_detail += "/" + rename_response->error().error_message;
                return command_result{error::vs_critical, error_detail};
            }
            case ErrorType::PATH_EXCEEDS_ROOT_PATH:
            case ErrorType::TARGET_PATH_EXCEEDS_ROOT_PATH:
            case ErrorType::PATH_DOES_NOT_EXISTS:
                return command_result{error::file_invalid_path};

            case ErrorType::TARGET_PATH_ALREADY_EXISTS:
                return command_result{error::file_already_exists};

            case ErrorType::SOME_FILES_ARE_LOCKED:
                return command_result{error::file_already_in_use, rename_response->error().error_message};
        }
    }

    serverInstance->action_logger()->file_logger.log_file_rename(this->getServerId(), this->ref(), channel_id, cmd["oldname"].string(), target_channel_id, cmd["newname"].string());
    return command_result{error::ok};
}

//clid=2 path=files\/virtualserver_1\/channel_5 name=image.iso
// size=673460224 sizedone=450756 clientftfid=2
// serverftfid=6 sender=0 status=1 current_speed=60872.8 average_speed runtime
command_result ConnectedClient::handleCommandFTList(ts::Command &cmd) {
    CMD_RESET_IDLE;
    CMD_REQ_SERVER;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);
    ACTION_REQUIRES_PERMISSION(permission::b_ft_transfer_list, 1, 0);

    auto virtual_file_server = file::server()->find_virtual_server(this->getServerId());
    if(!virtual_file_server) {
        return command_result{error::file_virtual_server_not_registered};
    }

    auto list_response = file::server()->file_transfer().list_transfer(); //FIXME: Only for the appropriate servers!
    if(!list_response->wait_for(kFileAPITimeout)) {
        return command_result{error::file_api_timeout};
    }

    if(!list_response->succeeded()) {
        using ErrorType = file::transfer::TransferListError;

        debugMessage(this->getServerId(), "{} Failed to list current transfers: {}", CLIENT_STR_LOG_PREFIX, (int) list_response->error());
        switch(list_response->error()) {
            case ErrorType::UNKNOWN: {
                auto error_detail = std::to_string((int) list_response->error());
                return command_result{error::vs_critical, error_detail};
            }
        }
    }


    const auto& transfers = list_response->response();
    if(transfers.empty()) {
        return command_result{error::database_empty_result};
    }

    ts::command_builder notify{this->notify_response_command("notifyftlist")};
    size_t bulk_index{0};
    for(const auto& transfer : transfers) {
        auto bulk = notify.bulk(bulk_index++);

        bulk.put_unchecked("clientftfid", transfer.client_transfer_id);
        bulk.put_unchecked("serverftfid", transfer.server_transfer_id);
        bulk.put_unchecked("sender", transfer.direction == file::transfer::Transfer::DIRECTION_DOWNLOAD);

        bulk.put_unchecked("clid", transfer.client_id);
        bulk.put_unchecked("cluid", transfer.client_unique_id);
        bulk.put_unchecked("path", transfer.file_path);
        bulk.put_unchecked("name", transfer.file_name);

        bulk.put_unchecked("size", transfer.expected_size);
        bulk.put_unchecked("sizedone", transfer.size_done);

        bulk.put_unchecked("status", (int) transfer.status);

        bulk.put_unchecked("runtime", transfer.runtime.count());
        bulk.put_unchecked("current_speed", transfer.current_speed);
        bulk.put_unchecked("average_speed", transfer.average_speed);
    }
    this->sendCommand(notify);

    return command_result{error::ok};
}

//ftstop serverftfid='2' clientftfid='4096' delete='0'
command_result ConnectedClient::handleCommandFTStop(ts::Command &cmd) {
    CMD_RESET_IDLE;
    CMD_REQ_SERVER;
    CMD_CHK_AND_INC_FLOOD_POINTS(25);

    auto virtual_file_server = file::server()->find_virtual_server(this->getServerId());
    if(!virtual_file_server) {
        return command_result{error::file_virtual_server_not_registered};
    }

    auto stop_response = file::server()->file_transfer().stop_transfer(virtual_file_server, cmd["serverftfid"], false);
    if(!stop_response->wait_for(kFileAPITimeout)) {
        return command_result{error::file_api_timeout};
    }

    if(!stop_response->succeeded()) {
        using ErrorType = file::transfer::TransferActionError::Type;
        switch (stop_response->error().error_type) {
            case ErrorType::UNKNOWN_TRANSFER:
                /* not known, so not stopped but it has the same result as it would have been stopped */
                return command_result{error::ok};

            case ErrorType::UNKNOWN: {
                auto error_detail = std::to_string((int) stop_response->error().error_type);
                if(!stop_response->error().error_message.empty()) {
                    error_detail += "/" + stop_response->error().error_message;
                }
                return command_result{error::vs_critical, error_detail};
            }
        }
    }
    return command_result{error::ok};
}







