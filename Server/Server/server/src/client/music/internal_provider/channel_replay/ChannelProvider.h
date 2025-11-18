#pragma once

#include <memory>
#include <teaspeak/MusicPlayer.h>

namespace music {
    namespace provider {
        class ChannelProvider : public manager::PlayerProvider  {
            public:
                static std::shared_ptr<::music::manager::PlayerProvider> create_provider();

                ChannelProvider() {
                    this->providerName = "ChannelProvider";
                    this->providerDescription = "Allows you to playback files uploaded within the channel or the music directory";
                }

                virtual ~ChannelProvider() = default;

                threads::Future<std::shared_ptr<UrlInfo>> query_info(const std::string &string, void *pVoid, void *pVoid1) override;

                threads::Future<std::shared_ptr<::music::MusicPlayer>> createPlayer(const std::string &string, void*, void*) override;

                bool acceptString(const std::string &str) override;
                std::vector<std::string> availableFormats() override;
                std::vector<std::string> availableProtocols() override;

            private:
        };
    }
}