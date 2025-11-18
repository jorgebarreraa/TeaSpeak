#pragma once

#include <cstdlib>
#include "Properties.h"
#include "PermissionManager.h"
#include "BasicChannel.h"
#include "../Group.h"
#include "../rtc/lib.h"
#include <memory>
#include <sql/SqlQuery.h>

namespace ts {
    namespace server {
        class VirtualServer;
        class ConnectedClient;
    }

    class ServerChannelTree;
    class ServerChannel : public BasicChannel {
            friend class ServerChannelTree;
        public:
            ServerChannel(uint32_t rtc_channel_id, ChannelId parentId, ChannelId channelId);
            ~ServerChannel() override;

            void setProperties(const std::shared_ptr<PropertyManager> &ptr) override;

            uint32_t rtc_channel_id;

            std::shared_mutex client_lock;
            std::deque<std::weak_ptr<server::ConnectedClient>> clients;

            void unregister_client(const std::shared_ptr<server::ConnectedClient>& /* client */);
            void register_client(const std::shared_ptr<server::ConnectedClient>& /* client */);

            bool deleted = false;
            size_t client_count();
    };

    class ServerChannelTree : public BasicChannelTree {
        public:
            ServerChannelTree(const std::shared_ptr<server::VirtualServer>&, sql::SqlManager*);
            ~ServerChannelTree() override;
            void loadChannelsFromDatabase();

            std::shared_ptr<BasicChannel> createChannel(ChannelId parentId, ChannelId orderId, const std::string &name) override;
            virtual std::deque<ChannelId> deleteChannelRoot(const std::shared_ptr<BasicChannel> &channel);

            void deleteSemiPermanentChannels();

            std::shared_ptr<LinkedTreeEntry> tree_head() { return this->head; }
        protected:
            ChannelId generateChannelId() override;

            void on_channel_entry_deleted(const std::shared_ptr<BasicChannel> &channel) override;

            std::shared_ptr<BasicChannel> allocateChannel(const std::shared_ptr<BasicChannel> &parent, ChannelId channelId) override;

        private:
            std::weak_ptr<server::VirtualServer> server_ref;
            ServerId getServerId();
            sql::SqlManager* sql;

            std::deque<std::shared_ptr<TreeView::LinkedTreeEntry>> tmpChannelList;

            bool initializeTempParents();
            bool buildChannelTreeFromTemp();
            bool updateOrderIds();
            bool validateChannelNames();
            bool validateChannelIcons();
            int loadChannelFromData(int argc, char** data, char** column);
    };
}