#pragma once

#include <string>
#include <memory>
#include <functional>

#include <Definitions.h>
#include <teaspeak/MusicPlayer.h>

namespace ts {
    namespace server {
        class VirtualServer;
    }

    namespace music {
        using MusicPlayer = ::music::MusicPlayer;

        class SongInfo {
            public:
                virtual ts::SongId getSongId() const = 0;
                virtual std::string getUrl() const = 0;
                virtual ts::ClientDbId  getInvoker() const = 0;
        };

        class PlayableSong : public SongInfo {
            public:
                struct StaticData {
                    ts::SongId _song_id;
                    std::string _song_url;
                    ts::ClientDbId _song_invoker;
                };

                struct LoadedData {
                    std::string title;
                    std::string description;
                    std::string thumbnail;
                    ::music::PlayerUnits length = ::music::PlayerUnits(0);

                    std::shared_ptr<MusicPlayer> player;
                };

                typedef std::function<std::shared_ptr<LoadedData>(const std::shared_ptr<server::VirtualServer>& /* server */, const std::shared_ptr<PlayableSong>& entry, std::string& error)> song_loader_t;
                typedef threads::Future<std::shared_ptr<LoadedData>> song_future_t;

            public:
                inline static std::shared_ptr<PlayableSong> create(StaticData data, std::shared_ptr<song_loader_t> _loaderFunction) {
                    struct shared_allocator : public PlayableSong { };

                    auto object = std::make_shared<shared_allocator>();
                    object->_self = object;
                    object->_loader_function = std::move(_loaderFunction);
                    object->data_static = std::move(data);
                    return object;
                }

                virtual ~PlayableSong();

                inline std::shared_ptr<song_loader_t> song_loader() { return this->_loader_function; }
                std::shared_ptr<song_future_t> get_loader(const std::shared_ptr<server::VirtualServer>&, bool /* create if not exists */ = true);
                inline std::shared_ptr<MusicPlayer> get_player() {
                    auto data = this->song_loaded_data();
                    if(!data) return nullptr;

                    return data->player;
                }

                [[nodiscard]] std::string getUrl() const override { return this->data_static._song_url; }
                [[nodiscard]] ts::ClientDbId  getInvoker() const override { return this->data_static._song_invoker; }
                [[nodiscard]] ts::SongId getSongId() const override  { return this->data_static._song_id; }

                [[nodiscard]] inline bool song_loaded() const { return this->data_loaded != nullptr; }
                [[nodiscard]] inline std::shared_ptr<LoadedData> song_loaded_data() const { return this->data_loaded; }
                [[nodiscard]] inline std::shared_ptr<PlayableSong> ref() { return this->_self.lock(); }

                inline void set_song_id(SongId id) {
                    this->data_static._song_id = id;
                }
            private:
                PlayableSong() = default;

                /* general */
                std::weak_ptr<PlayableSong> _self;

                /* static song data */
                StaticData data_static;
                std::shared_ptr<LoadedData> data_loaded;

                /* loader things */
                std::shared_ptr<song_loader_t> _loader_function;
                std::shared_ptr<song_future_t> _loader_future;

        };
    }
}