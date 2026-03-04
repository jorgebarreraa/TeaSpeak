#include <set>
#include <iomanip>
#include <netinet/in.h>
#include <log/LogUtils.h>
#include <misc/strobf.h>

#include "../InstanceHandler.h"
#include "../manager/ConversationManager.h"
#include "../music/MusicBotManager.h"
#include "../client/music/MusicClient.h"
#include "../client/voice/VoiceClient.h"

using namespace ts;
using namespace ts::server;
using namespace std;
using namespace std::chrono;

extern InstanceHandler *serverInstance;

std::deque<std::string> split(std::string str, std::string sep) {
    char *cstr = const_cast<char *>(str.c_str());
    char *current;
    std::deque<std::string> arr;
    current = strtok(cstr, sep.c_str());
    while (current != NULL) {
        arr.push_back(current);
        current = strtok(NULL, sep.c_str());
    }
    return arr;
}

#define ERR(sender, msg) \
do { \
    send_message(sender, msg); \
    return true; \
} while(false)

#define TLEN(index) if(arguments.size() < index) ERR(music_root, "Invalid argument count");
#define TARG(index, type) (arguments.size() > index && arguments[index] == type)
#define TMUSIC(bot) if(!bot->current_player()) ERR(music_root, "Im not playing a song!");
#define GBOT(var, ignore_disabled) \
auto var = this->selectedBot.lock(); \
if(!var) var = dynamic_pointer_cast<MusicClient>(target); \
if(!var) { \
    send_message(music_root, "Please select a music bot! (" + ts::config::music::command_prefix + "mbot select <id>)"); \
    return true; \
} \
if(!ignore_disabled && var->properties()[property::CLIENT_DISABLED].as<bool>()) { \
    send_message(music_root, strobf("This bot has been disabled. Upgrade your TeaSpeak license to use more bots.").string()); \
    return true; \
}

inline string filterUrl(string in){
    size_t index = 0;
    while ((index = in.find("[URL")) != std::string::npos && index < in.length()) {
        auto end = in.find(']', index);
        in.replace(index, end - index + 1, "", 0);
    }

    while ((index = in.find("[/URL")) != std::string::npos && index < in.length()) {
        auto end = in.find(']', index);
        in.replace(index, end - index + 1, "", 0);
    }
    return in;
}

inline void permissionableCommand(ConnectedClient* client, stringstream& ss, const std::string& cmd, permission::PermissionType perm, permission::PermissionType sec = permission::unknown) {
    auto permA = perm == permission::unknown || permission::v2::permission_granted(1, client->calculate_permission(perm, client->getChannelId()));
    auto permB = permA || (sec != permission::unknown && permission::v2::permission_granted(1, client->calculate_permission(sec, client->getChannelId())));
    if(!(permA || permB)) {
        ss << "[color=red]" << cmd << "[/color]" << endl;
    } else {
        ss << "[color=green]" << cmd << "[/color]" << endl;
    }
}

inline std::string bot_volume(float vol) {
    auto volume = to_string(vol * 100);
    auto idx = volume.find('.');
    return volume.substr(0, idx + 2);
}

//FIXME add chat command for channel commander (within bot settings)
//Return true if blocked
bool ConnectedClient::handleTextMessage(ChatMessageMode mode, std::string text, const std::shared_ptr<ConnectedClient>& target) {
    if (text.length() < ts::config::music::command_prefix.length()) return false;
    if (text.find(ts::config::music::command_prefix) != 0) return false;
    if(!this->currentChannel)
        return false;

    std::string command = text.substr(ts::config::music::command_prefix.length());
    auto arguments = command.find(' ') != -1 ? split(command.substr(command.find(' ') + 1), " ") : deque<string>{};
    command = command.substr(0, command.find(' '));
    debugMessage(this->getServerId(), "Having command \"" + command + "\".");
    for (const auto &arg : arguments)
        debugMessage(this->getServerId(), " Argument: '" + arg + "'");


    #define PERM_CHECK_BOT(perm, reqperm, err) \
    if(bot->properties()[property::CLIENT_OWNER] != this->getClientDatabaseId() &&  \
        !permission::v2::permission_granted(bot->calculate_permission(permission::reqperm, bot->getChannelId()), this->calculate_permission(permission::perm, bot->getChannelId()))) { \
        send_message(music_root, err); \
        return true; \
    }

//TODO: Correct error message print!
#define HANDLE_CMD_ERROR(_message) \
        send_message(music_root, string(_message) + ": action failed");
/*
    if(result.extraProperties.count("extra_msg") > 0) \
        send_message(music_root, string(_message) + ": " + result.extraProperties["extra_msg"]); \
    else if(result.extraProperties.count("failed_permid") > 0) \
        send_message(music_root, string(_message) + ". (Missing permission " + permission::resolvePermissionData((permission::PermissionType) stoull(result.extraProperties["failed_permid"]))->name + ")"); \
    else \
        send_message(music_root, string(_message) + ": " + result.error.message);
*/

#define JOIN_ARGS(variable, index)                                                                  \
    string variable;                                                                                \
    {                                                                                               \
        stringstream ss;                                                                            \
        for (auto it = arguments.begin() + index; it != arguments.end(); it++)                      \
            ss << *it << (it + 1 == arguments.end() ? "" : " ");                                    \
        variable = ss.str();                                                                        \
    }

    handle_text_command_fn_t function;
    if(mode == ChatMessageMode::TEXTMODE_SERVER) {
        function = [&](const shared_ptr<ConnectedClient>& sender, const string& message) {
            this->notifyTextMessage(ChatMessageMode::TEXTMODE_SERVER, sender, 0, 0, system_clock::now(), message);
        };
    } else if(mode == ChatMessageMode::TEXTMODE_CHANNEL) {
        function = [&](const shared_ptr<ConnectedClient>& sender, const string& message) {
            this->notifyTextMessage(ChatMessageMode::TEXTMODE_CHANNEL, sender, 0, 0, system_clock::now(), message);
        };
    } else if(mode == ChatMessageMode::TEXTMODE_PRIVATE) {
        function = [&, target](const shared_ptr<ConnectedClient>& sender, const string& message) {
            this->notifyTextMessage(ChatMessageMode::TEXTMODE_PRIVATE, target, this->getClientId(), 0, system_clock::now(), message);
        };

    }
    return handle_text_command(mode, command, arguments, function, target);
}

bool ConnectedClient::handle_text_command(
        ChatMessageMode mode,
        const string &command,
        const deque<string> &arguments,
        const function<void(const shared_ptr<ConnectedClient> &, const string &)> &send_message,
        const shared_ptr<ConnectedClient>& target) {

    auto music_root = dynamic_pointer_cast<ConnectedClient>(serverInstance->musicRoot());
    if (command == "mbot") {
        if(!config::music::enabled) {
            send_message(music_root, "Music bots are not enabled! Enable them via the server config!");
            return true;
        }
        if (TARG(0, "create")) {
            Command cmd("");
            auto result = this->handleCommandMusicBotCreate(cmd);
            if(result.has_error()) {
                result.release_data();
                HANDLE_CMD_ERROR("Failed to create music bot");
                return true;
            }
            send_message(music_root, "Bot created");
            return true;
        } else if (TARG(0, "list")) {
            bool server = false;
            if(TARG(1, "server"))
                server = true;

            string locationStr = server ? "on this server" : "in this channel";
            if(!permission::v2::permission_granted(1, this->calculate_permission(server ? permission::b_client_music_server_list : permission::b_client_music_channel_list, this->getChannelId()))) {
                send_message(music_root, "You don't have the permission to list all music bots " + locationStr);
                return true;
            }
            auto mbots =  this->server->getClientsByChannel<MusicClient>(server ? nullptr : this->currentChannel);
            if (mbots.empty())
                send_message(music_root, "There are no music bots " + locationStr);
            else {
                send_message(music_root, "There are " + to_string(mbots.size()) + " music bots " + locationStr + ":");
                for (const auto &mbot : mbots) {
                    if(mbot->properties()[property::CLIENT_DISABLED].as_or<bool>(false)) {
                        send_message(music_root, " - [color=red]" + to_string(mbot->getClientDatabaseId()) + " | " + mbot->getDisplayName() + " [DISABLED][/color]");
                    } else {
                        send_message(music_root, " - [color=green]" + to_string(mbot->getClientDatabaseId()) + " | " + mbot->getDisplayName() + "[/color]");
                    }
                }
            }
            return true;
        } else if (TARG(0, "select")) {
            TLEN(2);
            if(arguments[1].find_first_not_of("0123456789") != std::string::npos) {
                send_message(music_root, "Invalid bot id");
                return true;
            }
            auto botId = static_cast<ClientDbId>(stoll(arguments[1]));
            auto bot = this->server->music_manager_->findBotById(botId);
            if (!bot) ERR(music_root, "Could not find target bot");
            if(bot->properties()[property::CLIENT_OWNER] != this->getClientDatabaseId() &&
                    !permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_music_channel_list, this->getChannelId())) &&
                    !permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_music_server_list, this->getChannelId()))) { //No perms for listing
                send_message(music_root, "You don't have the permission to select a music bot");
                return true;
            }

            this->selectedBot = bot;
            send_message(music_root, "You successfully select the bot " + to_string(botId) + " | " + bot->getDisplayName());
            return true;
        } else if (TARG(0, "rename")) {
            TLEN(2);
            GBOT(bot, true);

            stringstream ss;
            for (auto it = arguments.begin() + 1; it != arguments.end(); it++)
                ss << *it << (it + 1 == arguments.end() ? "" : " ");
            string name = ss.str();

            Command cmd("");
            cmd["client_nickname"] = ss.str();
            auto result = this->handleCommandClientEdit(cmd, bot);
            if(result.has_error()) {
                HANDLE_CMD_ERROR("Failed to rename bot");
                result.release_data();
                return true;
            }
            send_message(music_root, "Name successfully changed!");
            return true;
        } else if (TARG(0, "delete")) {
            GBOT(bot, true);
            PERM_CHECK_BOT(i_client_music_delete_power, i_client_music_needed_delete_power, "You don't have the permission to rename this music bot");
            this->server->music_manager_->deleteBot(bot);
            send_message(bot, "You successfully deleted this music bot!");
            return true;
        } else if(TARG(0, "yt") || TARG(0, "soundcloud") || TARG(0, "sc")){
            TLEN(2);
            GBOT(bot, false);
            PERM_CHECK_BOT(i_client_music_play_power, i_client_music_needed_play_power, "You don't have the permission to play something this music bot");

            auto playlist = bot->playlist();
            if(!playlist) {
                send_message(bot, "bot hasnt a playlist!");
                return true;
            }

            JOIN_ARGS(url, 1);
            send_message(bot, "Queueing video " + url);
            playlist->add_song(this->ref(), filterUrl(url), "YouTube");
            return true;
        } else if(TARG(0, "stream")){
            TLEN(2);
            GBOT(bot, false);
            PERM_CHECK_BOT(i_client_music_play_power, i_client_music_needed_play_power, "You don't have the permission to play something this music bot");

            auto playlist = bot->playlist();
            if(!playlist) {
                send_message(bot, "bot hasnt a playlist!");
                return true;
            }

            JOIN_ARGS(url, 1);
            send_message(bot, "Queueing video " + url);
            playlist->add_song(this->ref(), filterUrl(url), "FFMpeg");
            return true;
        } else if(TARG(0, "player")) {
            TLEN(2);
            GBOT(bot, false);
            PERM_CHECK_BOT(i_client_music_play_power, i_client_music_needed_play_power, "You don't have the permission to play something this music bot");

            auto playlist = bot->playlist();
            if(!playlist) {
                send_message(bot, "bot hasnt a playlist!");
                return true;
            }

            JOIN_ARGS(url, 2);
            send_message(bot, "Queueing video " + url);
            playlist->add_song(this->ref(), filterUrl(url), arguments[1]);
            return true;
        } else if(TARG(0, "forward")){
            TLEN(2);
            GBOT(bot, false);
            PERM_CHECK_BOT(i_client_music_play_power, i_client_music_needed_play_power, "You don't have the permission to play something this music bot");
            TMUSIC(bot);

            if(arguments[1].find_first_not_of("0123456789") != std::string::npos) {
                send_message(bot, "Invalid number of seconds!");
                return true;
            }
            threads::Thread([bot, arguments]() {
                bot->current_player()->forward(seconds(stoll(arguments[1])));
            }).detach();
            send_message(bot, "Skipped " + to_string(stoll(arguments[1])) + " seconds!");
            return true;
        } else if(TARG(0, "rewind")){
            TLEN(2);
            GBOT(bot, false);
            PERM_CHECK_BOT(i_client_music_play_power, i_client_music_needed_play_power, "You don't have the permission to play something this music bot");
            TMUSIC(bot);

            if(arguments[1].find_first_not_of("0123456789") != std::string::npos) {
                send_message(bot, "Invalid number of seconds!");
                return true;
            }
            threads::Thread([bot, arguments]() {
                bot->current_player()->rewind(seconds(stoll(arguments[1])));
            }).detach();
            send_message(bot, "Rewind " + to_string(stoll(arguments[1])) + " seconds!");
            return true;
        } else if(TARG(0, "stop")){
            GBOT(bot, true);
            PERM_CHECK_BOT(i_client_music_play_power, i_client_music_needed_play_power, "You don't have the permission to play something this music bot");
            TMUSIC(bot);
            bot->current_player()->stop();
            send_message(bot, "Music stopped!");
            return true;
        } else if(TARG(0, "pause")){
            GBOT(bot, false);
            PERM_CHECK_BOT(i_client_music_play_power, i_client_music_needed_play_power, "You don't have the permission to play something this music bot");
            TMUSIC(bot);
            bot->current_player()->pause();
            send_message(bot, "Music paused!");
            return true;
        } else if(TARG(0, "play")) {
            if(arguments.size() >= 2) {
                GBOT(bot, false);
                PERM_CHECK_BOT(i_client_music_play_power, i_client_music_needed_play_power, "You don't have the permission to play something this music bot");

                auto playlist = bot->playlist();
                if(!playlist) {
                    send_message(bot, "bot hasnt a playlist!");
                    return true;
                }

                send_message(bot, "Queueing video " + arguments[1]);
                playlist->add_song(this->ref(), filterUrl(arguments[1]), "");
                return true;
            } else {
                GBOT(bot, false);
                PERM_CHECK_BOT(i_client_music_play_power, i_client_music_needed_play_power, "You don't have the permission to play something this music bot");
                TMUSIC(bot);
                bot->current_player()->play();
                send_message(bot, "Music started!");
                return true;
            }
        } else if(TARG(0, "info")){
            GBOT(bot, true);
            PERM_CHECK_BOT(i_client_music_info, i_client_music_needed_info, "You don't have the permission to display the music bot information");
            send_message(bot, "Music bot info:");
            send_message(bot, "   Bot id  : " + to_string(bot->getClientDatabaseId()));
            send_message(bot, "   Bot name: " + bot->getDisplayName());
            send_message(bot, "   Bot volume: " + bot_volume(bot->volumeModifier()));
            send_message(bot, "   State: " + to_string(bot->player_state()));
            if(bot->current_player()){
                auto player = bot->current_player();
                send_message(bot, "   Play state: player open");
                send_message(bot, "      State       : " + string(::music::stateNames[player->state()]));
                send_message(bot, "      Title       : " + player->songTitle());
                send_message(bot, "      Description : " + player->songDescription());
                send_message(bot, "      Timeline    : " + to_string(duration_cast<seconds>(player->currentIndex()).count()) + "/" + to_string(duration_cast<seconds>(player->length()).count()));
                send_message(bot, "      Buffered    : " + to_string(duration_cast<seconds>(player->bufferedUntil() - player->currentIndex()).count()) + " seconds");
            } else {
                send_message(bot, "   Play state: not playing");
            }

            auto bot_playlist = bot->playlist();
            if(bot_playlist) {
                send_message(bot, "   Playlist ID    : " + to_string(bot_playlist->playlist_id()));
                send_message(bot, "   Playlist size  : " + to_string(bot_playlist->list_songs().size()));
            } else {
                send_message(bot, "   Playlist ID    : No playlist assigned");
            }
            return true;
        } else if(TARG(0, "queue") || TARG(0, "playlist") || TARG(0, "pl")) {
            GBOT(bot, false);
            PERM_CHECK_BOT(i_client_music_info, i_client_music_needed_info, "You don't have the permission to display the music bot information");

            auto bot_playlist = bot->playlist();
            if(bot_playlist) {
                send_message(bot, "Playlist ID    : " + to_string(bot_playlist->playlist_id()));
                send_message(bot, "Playlist entries (" + to_string(bot_playlist->list_songs().size()) + "): [color=orange]orange[/color] = currently index");

                set<ClientDbId> dbids;
                for(const auto& song : bot_playlist->list_songs())
                    dbids.insert(song->invoker);

                auto dbinfo = serverInstance->databaseHelper()->queryDatabaseInfo(this->getServer(), deque<ClientDbId>(dbids.begin(), dbids.end()));

                auto current_song = bot_playlist->currently_playing();
                for(const auto& song : bot_playlist->list_songs()) {
                    string invoker = "unknown";
                    for(const auto& e : dbinfo) {
                        if(e->client_database_id == song->invoker) {
                            invoker = "[URL=client://0/" + e->client_unique_id + "~" + e->client_nickname + "]" + e->client_nickname + "[/URL]";
                            break;
                        }
                    }

                    string data = "\"" + song->original_url + "\" added by " + invoker;
                    if(song->song_id == current_song)
                        data = "[color=orange]" + data + "[/color]";
                    send_message(bot, " - " + data);
                }

            } else {
                send_message(bot, "The bot hasn't a playlist");
            }
            return true;
        } else if(TARG(0, "next")) {
            GBOT(bot, false);
            PERM_CHECK_BOT(i_client_music_play_power, i_client_music_needed_play_power, "You don't have the permission to play something this music bot");
            bot->forwardSong();
            auto song = bot->current_song();
            if(song)
                send_message(bot, "Replaying next song (" + song->getUrl() + ")");
            else send_message(bot, "Queue is empty! Could not forward!");
            return true;
        } else if(TARG(0, "volume")) {
            GBOT(bot, false);
            PERM_CHECK_BOT(i_client_music_play_power, i_client_music_needed_play_power, "You don't have the permission to play something this music bot");
            if(arguments.size() < 2) {
                send_message(bot, "Current volume: " + bot_volume(bot->volumeModifier()));
                return true;
            }
            if(arguments[1].find_first_not_of(".-0123456789") != std::string::npos) {
                send_message(bot, "Invalid volume!");
                return true;
            }
            auto volume = stof(arguments[1]);
            if(volume < 0 || volume > 100) {
                send_message(bot, "Invalid volume! Volume must be greater or equal to zero and less or equal then one!");
                return true;
            }

            auto max_volume = this->calculate_permission(permission::i_client_music_create_modify_max_volume, 0);
            if(max_volume.has_value && !permission::v2::permission_granted(volume, max_volume)) {
                send_message(bot, "You don't have the permission to use higher volumes that " + to_string(max_volume.value) + "%");
                return true;
            }

            bot->volume_modifier(volume / 100);
            send_message(bot, "Volume successfully changed to " + bot_volume(bot->volumeModifier()));
            return true;
        } else if(TARG(0, "formats")) {
            auto providers = ::music::manager::registeredTypes();
            stringstream ss;
            ss << "Available providers:" << endl;
            for(const auto& prov : providers) {
                ss << "  " << prov->providerName << ":" << endl;
                ss << "    Description: " << prov->providerDescription << endl;

                auto fmts = prov->availableFormats();
                ss << "    Supported formats: (" << fmts.size() << ")" << endl;
                for(const auto& fmt : fmts)
                    ss << "    - " << fmt << endl;

                auto prots = prov->availableProtocols();
                ss << "    Supported protocols: (" << prots.size() << ")" << endl;
                for(const auto& fmt : prots)
                    ss << "    - " << fmt << endl;
            }
            send_message(music_root, ss.str());
            return true;
        } else if(TARG(0, "settings")) {
            GBOT(bot, false);
            PERM_CHECK_BOT(i_client_music_play_power, i_client_music_needed_play_power, "You don't have the permission to change anything on the bot"); //TODO FIXME!

            if(TARG(1, "bot")) {

                const static vector<string> editable_properties = {
                        "client_nickname",
                        "client_player_volume",
                        "client_is_channel_commander",
                        "client_version",
                        "client_country",
                        "client_platform",
                        "client_bot_type",
                        "client_uptime_mode",
                        "client_is_priority_speaker",
                        "client_flag_notify_song_change"
                };

                if(arguments.size() < 3) {
                    send_message(bot, "Bot properties:");
                    for(const auto& property : bot->properties()->list_properties(~0)) {
                        if(find(editable_properties.begin(), editable_properties.end(), property.type().name) == editable_properties.end()) continue;

                        send_message(bot, " - " + std::string{property.type().name} + " = " + property.value() + " " + (property.default_value() == property.value() ? "(default)" : ""));
                    }
                } else if(arguments.size() < 4) {
                    if(find(editable_properties.begin(), editable_properties.end(), arguments[2]) == editable_properties.end()) {
                        send_message(bot, "Unknown property or property is not editable.");
                        return true;
                    }

                    const auto &property_info = property::find<property::ClientProperties>(arguments[2]);
                    if(property_info.is_undefined()) {
                        send_message(bot, "Unknown property " + arguments[2] + ".");
                        return true;
                    }

                    auto prop = bot->properties()[(property::ClientProperties) property_info.property_index];
                    send_message(bot, "Bot property " + std::string{property_info.name} + " = " + prop.value() + " " + (property_info.default_value == prop.value() ? "(default)" : ""));
                    return true;
                } else {
                    Command cmd("");
                    JOIN_ARGS(value, 3);
                    cmd[arguments[2]] = value;
                    auto result = this->handleCommandClientEdit(cmd, bot);
                    if(result.has_error()) {
                        HANDLE_CMD_ERROR("Failed to change bot property");
                        result.release_data();
                        return true;
                    }
                    send_message(music_root, "Property successfully changed!");
                    return true;
                }

                return true;
            } else if(TARG(1, "playlist")) {
                auto playlist = bot->playlist();
                if(!playlist) {
                    send_message(bot, "Bot hasn't a playlist!");
                    return true;
                }
                if(arguments.size() < 3) {
                    send_message(bot, "Playlist properties:");
                    for(const auto& property : playlist->properties().list_properties(property::FLAG_PLAYLIST_VARIABLE)) {
                        send_message(bot, " - " + std::string{property.type().name} + " = " + property.value() + " " + (property.default_value() == property.value() ? "(default)" : ""));
                    }
                } else if(arguments.size() < 4) {
                    const auto &property_info = property::find<property::PlaylistProperties>(arguments[2]);
                    if(property_info.is_undefined()) {
                        send_message(bot, "Unknown property " + arguments[2] + ".");
                        return true;
                    }

                    auto prop = playlist->properties()[(property::PlaylistProperties) property_info.property_index];
                    send_message(bot, "Bot property " + std::string{property_info.name} + " = " + prop.value() + " " + (property_info.default_value == prop.value() ? "(default)" : ""));
                } else {
                    const auto &property_info = property::find<property::PlaylistProperties>(arguments[2]);
                    if(property_info.is_undefined()) {
                        send_message(bot, "Unknown property " + arguments[2] + ".");
                        return true;
                    }

                    JOIN_ARGS(value, 3);
                    if(!property_info.validate_input(value)) {
                        send_message(bot, "Please enter a valid value!");
                        return true;
                    }

                    if((property_info.flags & property::FLAG_USER_EDITABLE) == 0) {
                        send_message(bot, "This property isnt changeable!");
                        return true;
                    }

                    playlist->properties()[(property::PlaylistProperties) property_info.property_index] = value;
                    send_message(bot, "Property successfully changed");
                    return true;
                }
            } else {
                send_message(bot, "Please enter a valid setting mode");
            }
            return true;
        }
    } else if (command == "help") {
        //send_message(music_root, "  ̶.̶̶m̶̶b̶̶o̶̶t̶̶ ̶̶f̶̶o̶̶r̶̶m̶̶a̶̶t̶̶s (Not supported yet)");
        stringstream ss;
        ss << "Available music bot commands: ([color=green]green[/color] = permission granted | [color=red]red[/color] = insufficient permissions)" << endl;

        bool has_list_server = permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_music_server_list, this->getChannelId()));
        bool has_list_channel = permission::v2::permission_granted(1, this->calculate_permission(permission::b_client_music_channel_list, this->getChannelId()));
        permissionableCommand(this, ss, string() + "  .mbot list [<[color=" + (has_list_server ? "green" : "red") + "]server[/color]|[color=" + (has_list_channel ? "green" : "red") + "]channel[/color]>]", permission::b_client_music_channel_list, permission::b_client_music_server_list);
        permissionableCommand(this, ss, "  " + ts::config::music::command_prefix + "mbot select <id>", permission::b_client_music_channel_list, permission::b_client_music_server_list);
        permissionableCommand(this, ss, "  " + ts::config::music::command_prefix + "mbot formats", permission::unknown);

        permissionableCommand(this, ss, "  " + ts::config::music::command_prefix + "mbot create", permission::b_client_music_create_temporary); // [<type>]
        permissionableCommand(this, ss, "  " + ts::config::music::command_prefix + "mbot info", permission::i_client_music_info);
        permissionableCommand(this, ss, "  " + ts::config::music::command_prefix + "mbot rename", permission::i_client_music_rename_power);
        permissionableCommand(this, ss, "  " + ts::config::music::command_prefix + "mbot delete", permission::i_client_music_delete_power);

        permissionableCommand(this, ss, "  " + ts::config::music::command_prefix + "mbot yt <video url>", permission::i_client_music_play_power);
        permissionableCommand(this, ss, "  " + ts::config::music::command_prefix + "mbot soundcloud <video url>", permission::i_client_music_play_power);
        permissionableCommand(this, ss, "  " + ts::config::music::command_prefix + "mbot stream <stream url> (For supported protocols|formats see '" + ts::config::music::command_prefix + "mbot formats')", permission::i_client_music_play_power);
        permissionableCommand(this, ss, "  " + ts::config::music::command_prefix + "mbot player <provider> <data>", permission::i_client_music_play_power);

        permissionableCommand(this, ss, "  " + ts::config::music::command_prefix + "mbot play", permission::i_client_music_play_power);
        permissionableCommand(this, ss, "  " + ts::config::music::command_prefix + "mbot play <url>", permission::i_client_music_play_power);
        permissionableCommand(this, ss, "  " + ts::config::music::command_prefix + "mbot volume <0-100>", permission::i_client_music_play_power);
        permissionableCommand(this, ss, "  " + ts::config::music::command_prefix + "mbot forward <seconds>", permission::i_client_music_play_power);
        permissionableCommand(this, ss, "  " + ts::config::music::command_prefix + "mbot rewind <seconds>", permission::i_client_music_play_power);
        permissionableCommand(this, ss, "  " + ts::config::music::command_prefix + "mbot pause", permission::i_client_music_play_power);
        permissionableCommand(this, ss, "  " + ts::config::music::command_prefix + "mbot stop", permission::i_client_music_play_power);
        permissionableCommand(this, ss, "  " + ts::config::music::command_prefix + "mbot next", permission::i_client_music_play_power);
        permissionableCommand(this, ss, "  " + ts::config::music::command_prefix + "mbot <playlist|pl>", permission::i_client_music_play_power);
        permissionableCommand(this, ss, "  " + ts::config::music::command_prefix + "mbot settings <playlist|bot> [name] [value]", permission::i_client_music_modify_power);

        send_message(music_root, ss.str());
        return true;
    } else if (command == "dummy") {
        if(TARG(0, "timerevent")) {
            send_message(this->ref(), "Sending command dummy_timerevent");
            this->sendCommand(Command("dummy_timerevent"));
            return true;
        } else if(TARG(0, "rd")) {
            send_message(this->ref(), "Sending command rd");
            Command cmd("rd");
            cmd["msg"] = "Hello world";
            this->sendCommand(cmd);
            return true;
        } else if(TARG(0, "connectfailed")) {
            send_message(this->ref(), "Sending command dummy_connectfailed");
            Command cmd("dummy_connectfailed");
            cmd["status"] = 1797;
            this->sendCommand(cmd);
            return true;
        }

        send_message(this->ref(), "Invalid dummy command!");
        send_message(this->ref(), "- timerevent");
        send_message(this->ref(), "- connectfailed");
        send_message(this->ref(), "- rd");
        return true;
    } else if(command == "protocol") {
        if(TARG(0, "generation")) {
            auto vc = dynamic_pointer_cast<VoiceClient>(this->ref());
            if(!vc) return false;

            send_message(this->ref(), "Packet generations:");
            /*
            for(const auto& type : {
                    protocol::PacketTypeInfo::Command,
                    protocol::PacketTypeInfo::CommandLow,
                    protocol::PacketTypeInfo::Ack,
                    protocol::PacketTypeInfo::AckLow,
                    protocol::PacketTypeInfo::Voice,
                    protocol::PacketTypeInfo::VoiceWhisper,
                    protocol::PacketTypeInfo::Ping,
                    protocol::PacketTypeInfo::Pong}) {

                //auto id = vc->getConnection()->getPacketIdManager().currentPacketId(type);
                //auto gen = vc->getConnection()->getPacketIdManager().generationId(type);
                //auto& genestis = vc->getConnection()->get_incoming_generation_estimators();

                //send_message(this->ref(), " OUT " + type.name() + " => generation: " + to_string(gen) + " id: " + to_string(id));
                //send_message(this->ref(), " IN  " + type.name() + " => generation: " + to_string(genestis[type.type()].generation()) + " id: " + to_string(genestis[type.type()].current_packet_id()));
            }
            */
            return true;
        } else if(TARG(0, "rtt")) {
            auto vc = dynamic_pointer_cast<VoiceClient>(this->ref());
            if(!vc) return false;

            auto& ack = vc->connection->packet_encoder().acknowledge_manager();
            send_message(this->ref(), "Command retransmission values:");
            send_message(this->ref(), " RTO   : " + std::to_string(ack.current_rto()));
            send_message(this->ref(), " RTTVAR: " + std::to_string(ack.current_rttvar()));
            send_message(this->ref(), " SRTT  : " + std::to_string(ack.current_srtt()));
            return true;
        } else if(TARG(0, "sgeneration")) {
            TLEN(4);

            try {
                /*
                auto type = stol(arguments[1]);
                auto generation = stol(arguments[2]);
                auto pid = stol(arguments[3]);

                auto vc = dynamic_pointer_cast<VoiceClient>(this->ref());
                if(!vc) return false;

                auto& genestis = vc->getConnection()->get_incoming_generation_estimators();
                if(type >= genestis.size()) {
                    send_message(this->ref(), "Invalid type");
                    return true;
                }
                genestis[type].set_last_state(pid, generation);
                */
            } catch(std::exception& ex) {
                send_message(this->ref(), "Failed to parse argument");
                return true;
            }
            return true;
        } else if(TARG(0, "ping")) {
            auto vc = dynamic_pointer_cast<VoiceClient>(this->ref());
            if(!vc) return false;

            auto ping_handler = vc->getConnection()->ping_handler();
            send_message(this->ref(), "Ping: " + std::to_string(ping_handler.current_ping().count()));

            auto last_response_ms = std::chrono::ceil<std::chrono::milliseconds>(std::chrono::system_clock::now() - ping_handler.last_ping_response()).count();
            send_message(this->ref(), "Last ping response: " + std::to_string(last_response_ms));
            return true;
        } else if(TARG(0, "disconnect")) {
            auto vc = dynamic_pointer_cast<VoiceClient>(this->ref());
            if(!vc) return false;

            send_message(this->ref(), "You'll timeout");
            Command cmd("notifyclientupdated");
            vc->sendCommand(cmd);
            return true;
        } else if(TARG(0, "resetip")) {
            auto vc = dynamic_pointer_cast<VoiceClient>(this->ref());
            if(!vc) return false;

            send_message(this->ref(), "I lost your IP address. I'm so dump :)");
            vc->connection->reset_remote_address();
            memset(&vc->remote_address, 0, sizeof(vc->remote_address));
            send_message(this->ref(), "Hey, we got the address back");
            return true;
        } else if(TARG(0, "fb")) {
            this->increaseFloodPoints(0xFF8F);
            send_message(this->ref(), "Done :)");
            return true;
        } else if(TARG(0, "binary")) {
            send_message(this->ref(), "Send binary message");
            this->sendCommand(Command{"\02\03\04 \22"});
            return true;
        }
        send_message(this->ref(), "Invalid protocol command!");
        send_message(this->ref(), "- generation");
        send_message(this->ref(), "- disconnect");
        send_message(this->ref(), "- resetip");
        send_message(this->ref(), "- fb");
        return true;
    } else if (command == "sleep") {
        if(arguments.empty() || arguments[0].find_first_not_of("0123456789") != string::npos) {
            send_message(this->ref(), "Invalid argument! Requires a number in ms.");
            return true;
        }

        send_message(this->ref(), "Sleeping for " + to_string(stoll(arguments[0])) + "!");
        auto end = system_clock::now() + milliseconds(stoll(arguments[0]));
        threads::self::sleep_until(end);
        send_message(this->ref(), "Done!");
        return true;
    } else if(command == "conversation") {
        if(TARG(0, "history")) {
            system_clock::time_point timestamp_begin = system_clock::now();
            system_clock::time_point timestamp_end;
            size_t message_count = 100;

            if(arguments.size() > 1) {
                timestamp_begin -= seconds(stoll(arguments[1]));
            }
            if(arguments.size() > 2) {
                timestamp_end = system_clock::now() - seconds(stoll(arguments[2]));
            }
            if(arguments.size() > 3) {
                message_count = stoll(arguments[3]);
            }

            auto time_str = [](const system_clock::time_point& tp) {
                auto seconds_since_epoch = std::chrono::system_clock::to_time_t(tp);

                ostringstream os;
                os << std::put_time(std::localtime(&seconds_since_epoch), "%Y %b %d %H:%M:%S");
                return os.str();
            };
            send_message(this->ref(), "Looking up history from " + time_str(timestamp_end) + " to " + time_str(timestamp_begin) + ". Max messages: " + to_string(message_count));
            auto conversation = this->server->conversation_manager()->get_or_create(this->currentChannel->channelId());
            auto data = conversation->message_history(timestamp_begin, message_count, timestamp_end);
            send_message(this->ref(), "Entries: " + to_string(data.size()));
            for(auto& entry : data) {
                send_message(this->ref(), "<" + time_str(entry->message_timestamp) + ">" + entry->sender_name + ": " + entry->message);
            }
            return true;
        }
    }

    send_message(music_root, "Invalid channel command.");
    send_message(music_root, "Type " + ts::config::music::command_prefix + "help for a command overview");

    return false;
}