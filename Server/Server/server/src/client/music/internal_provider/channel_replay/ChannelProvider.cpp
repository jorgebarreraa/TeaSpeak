#include "ChannelProvider.h"
#include "../../MusicClient.h"
#include "../../../../InstanceHandler.h"
#include "../../../../../../music/providers/ffmpeg/FFMpegProvider.h"


using namespace std;
using namespace music::provider;
using namespace music::manager;
using namespace music;
using namespace ts;
using namespace ts::server;

struct AudioFileInfo {
    std::string absolute_path;
    std::string title;
    std::string description;
};

threads::Future<std::shared_ptr<::music::MusicPlayer>> ChannelProvider::createPlayer(const std::string &url, void*, void* ptr_server) {
    auto server = ((VirtualServer*) ptr_server)->ref();
    threads::Future<std::shared_ptr<::music::MusicPlayer>> future;

#if 0
    if(server) {
        std::thread([future, server, url, ptr_server]{
            auto f_server = serverInstance->getFileServer();
            if(!f_server) {
                future.executionFailed("instance missing file server");
                return;
            }

            Command command("");
            try {
                command = Command::parse(pipes::buffer_view{url.data(), url.length()}, false);
            } catch(std::exception& ex) {
                future.executionFailed("failed to parse data");
                return;
            }

            try {
                auto channel_id = command["cid"].as<ChannelId>();
                auto name = command["name"].string();
                auto path = command["path"].string();

                auto channel = server->getChannelTree()->findChannel(channel_id);
                if(!channel) {
                    future.executionFailed("could not find target channel");
                    return;
                }

                auto directory = f_server->resolveDirectory(server, channel, path);
                if(!directory) {
                    future.executionFailed("invalid file path");
                    return;
                }

                auto file = f_server->findFile(name, directory);
                if(!file) {
                    future.executionFailed("could not find file");
                    return;
                }

                if(file->type != file::FileType::FILE) {
                    future.executionFailed("file isnt a file");
                    return;
                }

                auto info = make_shared<AudioFileInfo>();
                info->absolute_path = file->path + "/" + file->name;
                info->title = file->name; /* fallback */
                info->description = "File from channel " + channel->name() + " at " + path; /* fallback */
                auto ffmpeg = ::music::manager::resolveProvider("FFMpeg", "");
                if(!ffmpeg) {
                    future.executionFailed("missing ffmpeg");
                    return;
                }

                {
                    auto ffmpeg_data = (::music::FFMpegData::FileReplay*) malloc(sizeof(::music::FFMpegData::FileReplay));
                    if(!ffmpeg_data) {
                        future.executionFailed("failed to allocate memory");
                        return;
                    }
                    memset(ffmpeg_data, 0, sizeof(::music::FFMpegData::FileReplay));

                    ffmpeg_data->version = 2;
                    ffmpeg_data->type = ::music::FFMpegData::REPLAY_FILE;
                    ffmpeg_data->_free = ::free;

                    ffmpeg_data->file_path = (char*) malloc(info->absolute_path.length() + 1);
                    ffmpeg_data->file_path[info->absolute_path.length()] = '\0';
                    ffmpeg_data->file_title = (char*) malloc(info->title.length() + 1);
                    ffmpeg_data->file_title[info->title.length()] = '\0';
                    ffmpeg_data->file_description = (char*) malloc(info->description.length() + 1);
                    ffmpeg_data->file_description[info->description.length()] = '\0';

                    memcpy(ffmpeg_data->file_title, info->title.data(), info->title.length());
                    memcpy(ffmpeg_data->file_path, info->absolute_path.data(), info->absolute_path.length());
                    memcpy(ffmpeg_data->file_description, info->description.data(), info->description.length());

                    auto p = ffmpeg->createPlayer("", ffmpeg_data, ptr_server);
                    p.wait();
                    if(p.failed())
                        future.executionFailed(p.errorMegssage());
                    else {
                        future.executionSucceed(*p.get());
                    }
                }
                return;
            } catch(std::exception& ex) {
                future.executionFailed("failed to process data");
                return;
            }
        }).detach();
    } else {
        future.executionFailed("invalid bot");
    }
#else
    future.executionFailed("channel file playback is currently not supported");
#endif

    return future;
}

bool ChannelProvider::acceptString(const std::string &str) {
    if(str.find("cid=") == string::npos) return false;
    if(str.find("path=") == string::npos) return false;
    if(str.find("name=") == string::npos) return false;

    return true;
}

vector<string> ChannelProvider::availableFormats() {
    return {};
}

vector<string> ChannelProvider::availableProtocols() {
    return {"query"};
}

std::shared_ptr<::music::manager::PlayerProvider> ChannelProvider::create_provider() {
    return std::shared_ptr<ChannelProvider>(new ChannelProvider(), [](ChannelProvider* provider){
        if(!provider) return;
        delete provider;
    });
}

#if 0
threads::Future<shared_ptr<UrlInfo>> ChannelProvider::query_info(const std::string &string, void *pVoid, void *pVoid1) {
    auto info = make_shared<UrlSongInfo>();


    info->type = UrlType::TYPE_VIDEO;
    info->url = string;
    info->title = "";
    info->description = "";
    info->metadata = {};

    threads::Future<shared_ptr<UrlInfo>> result;
    result.executionSucceed(info);
    return result;
}
#endif

threads::Future<shared_ptr<UrlInfo>> ChannelProvider::query_info(const std::string &url, void*, void* ptr_server) {
    auto server = ((VirtualServer*) ptr_server)->ref();
    threads::Future<shared_ptr<UrlInfo>> future;

#if 0
    if(server) {
        std::thread([future, server, url, ptr_server]{
            auto f_server = serverInstance->getFileServer();
            if(!f_server) {
                future.executionFailed("instance missing file server");
                return;
            }

            Command command("");
            try {
                command = Command::parse(pipes::buffer_view{url.data(), url.length()}, false);
            } catch(std::exception& ex) {
                future.executionFailed("failed to parse data");
                return;
            }

            try {
                auto channel_id = command["cid"].as<ChannelId>();
                auto name = command["name"].string();
                auto path = command["path"].string();

                auto channel = server->getChannelTree()->findChannel(channel_id);
                if(!channel) {
                    future.executionFailed("could not find target channel");
                    return;
                }

                auto directory = f_server->resolveDirectory(server, channel, path);
                if(!directory) {
                    future.executionFailed("invalid file path");
                    return;
                }

                auto file = f_server->findFile(name, directory);
                if(!file) {
                    future.executionFailed("could not find file");
                    return;
                }

                if(file->type != file::FileType::FILE) {
                    future.executionFailed("file isnt a file");
                    return;
                }

                auto info = make_shared<AudioFileInfo>();
                info->absolute_path = file->path + "/" + file->name;
                info->title = file->name; /* fallback */
                info->description = "File from channel " + channel->name() + " at " + file->path; /* fallback */

                auto ffmpeg = ::music::manager::resolveProvider("FFMpeg", "");
                if(!ffmpeg) {
                    future.executionFailed("missing ffmpeg");
                    return;
                }

                {
                    auto ffmpeg_data = (::music::FFMpegData::FileReplay*) malloc(sizeof(::music::FFMpegData::FileReplay));
                    if(!ffmpeg_data) {
                        future.executionFailed("failed to allocate memory");
                        return;
                    }
                    memset(ffmpeg_data, 0, sizeof(::music::FFMpegData::FileReplay));

                    ffmpeg_data->version = 2;
                    ffmpeg_data->type = ::music::FFMpegData::REPLAY_FILE;
                    ffmpeg_data->_free = ::free;

                    ffmpeg_data->file_path = (char*) malloc(info->absolute_path.length() + 1);
                    ffmpeg_data->file_path[info->absolute_path.length()] = '\0';
                    ffmpeg_data->file_title = (char*) malloc(info->title.length() + 1);
                    ffmpeg_data->file_title[info->title.length()] = '\0';
                    ffmpeg_data->file_description = (char*) malloc(info->description.length() + 1);
                    ffmpeg_data->file_description[info->description.length()] = '\0';

                    memcpy(ffmpeg_data->file_title, info->title.data(), info->title.length());
                    memcpy(ffmpeg_data->file_path, info->absolute_path.data(), info->absolute_path.length());
                    memcpy(ffmpeg_data->file_description, info->description.data(), info->description.length());

                    auto p = ffmpeg->query_info("", ffmpeg_data, ptr_server);
                    if(!p.wait(std::chrono::system_clock::now() + std::chrono::seconds{30}))
                        future.executionFailed("ffmpeg load timeout");
                    else if(p.failed())
                        future.executionFailed(p.errorMegssage());
                    else {
                        auto result = *p.get();
                        result->url = url;
                        future.executionSucceed(result);
                    }
                }
                return;
            } catch(std::exception& ex) {
                future.executionFailed("failed to process data");
                return;
            }
        }).detach();
    } else {
        future.executionFailed("invalid bot");
    }

#else
    future.executionFailed("channel file playback is currently not supported");
#endif

    return future;
}