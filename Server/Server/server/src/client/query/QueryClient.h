#pragma once

#include <src/client/ConnectedClient.h>
#include <poll.h>
#include <protocol/buffers.h>
#include <pipes/ssl.h>
#include "misc/queue.h"
#include "../shared/ServerCommandExecutor.h"

namespace ts::server {
    class QueryServer;
    class QueryAccount;
    class QueryClientCommandHandler;

    namespace query {
        struct NetworkBuffer {
            static NetworkBuffer* allocate(size_t /* length */);

            size_t length;
            size_t bytes_written{0};
            NetworkBuffer* next_buffer{nullptr};

            std::atomic_int16_t ref_count{};

            [[nodiscard]] inline const void* data() const { return (const char*) this + sizeof(NetworkBuffer); }
            [[nodiscard]] inline void* data() { return (char*) this + sizeof(NetworkBuffer); }

            [[nodiscard]] NetworkBuffer* ref();
            void unref();
        };
    }

    class QueryClient : public ConnectedClient {
            friend class QueryServer;
            friend class QueryClientCommandHandler;

            using NetworkBuffer = query::NetworkBuffer;

            enum ConnectionType {
                PLAIN,
                SSL_ENCRYPTED,
                UNKNOWN
            };
        public:
            QueryClient(QueryServer* /* server handle */, int /* file descriptor */);
            ~QueryClient() override;

            void sendCommand(const ts::Command &command, bool low = false) override;
            void sendCommand(const ts::command_builder &command, bool low) override;

            bool disconnect(const std::string &reason) override;
            bool close_connection(const std::chrono::system_clock::time_point& flush_timeout) override;

            bool eventActive(QueryEventGroup, QueryEventSpecifier);
            void toggleEvent(QueryEventGroup, QueryEventSpecifier, bool);
            void resetEventMask();

            bool ignoresFlood() override;
            void disconnect_from_virtual_server(const std::string& /* reason */);

            inline std::shared_ptr<QueryAccount> getQueryAccount() { return this->query_account; }
        protected:
            void initialize_weak_reference(const std::shared_ptr<ConnectedClient> &) override;

            void preInitialize();
            void initializeSSL();
            void postInitialize();

            /* Will be called by the query server */
            void execute_final_disconnect();

            /* the ticking method will only be called when connected to a server */
            void tick_server(const std::chrono::system_clock::time_point &time) override;
            void tick_query();

            /* Methods will be called within the io loop (single thread) */
            static void handle_event_read(int, short, void*);
            void handle_message_read(const std::string_view& /* message */);
            void handle_decoded_message(const std::string_view& /* message */);

            /* Methods will be called within the io loop (single thread) */
            static void handle_event_write(int, short, void*);
            void send_message(const std::string_view&);
            void enqueue_write_buffer(const std::string_view& /* message */);
        private:
            QueryServer* handle;

            ConnectionType connectionType{ConnectionType::UNKNOWN};

            bool whitelisted{false};
            int client_file_descriptor{-1};

            spin_mutex network_mutex{};
            ::event* event_read{nullptr};
            ::event* event_write{nullptr};

            /* locked by network_mutex */
            NetworkBuffer* write_buffer_head{nullptr};
            NetworkBuffer** write_buffer_tail{&this->write_buffer_head};

            /* pipes::SSL internally thread save */
            pipes::SSL ssl_handler;

            std::chrono::system_clock::time_point flush_timeout{};

            /* The line buffer must only be accessed within the io event loop! */
            char* line_buffer{nullptr};
            size_t line_buffer_length{0};
            size_t line_buffer_capacity{0};
            size_t line_buffer_scan_offset{0};

            /* thread save to access */
            std::unique_ptr<ServerCommandQueue> command_queue{};

            std::chrono::time_point<std::chrono::system_clock> connectedTimestamp;
            uint16_t eventMask[QueryEventGroup::QEVENTGROUP_MAX];

            std::shared_ptr<QueryAccount> query_account;
        protected:
            command_result handleCommand(Command &command) override;

        public:
            //Silent events
            bool notifyClientNeededPermissions() override;
            bool notifyChannelSubscribed(const std::deque<std::shared_ptr<BasicChannel>> &deque) override;
            bool notifyChannelUnsubscribed(const std::deque<std::shared_ptr<BasicChannel>> &deque) override;

            bool notifyServerUpdated(std::shared_ptr<ConnectedClient> ptr) override;
            bool notifyClientPoke(std::shared_ptr<ConnectedClient> invoker, std::string msg) override;

            bool notifyClientUpdated(const std::shared_ptr<ConnectedClient> &ptr, const std::deque<const property::PropertyDescription*> &deque, bool lock_channel_tree) override;

            bool notifyPluginCmd(std::string name, std::string msg,std::shared_ptr<ConnectedClient>) override;
            bool notifyClientChatComposing(const std::shared_ptr<ConnectedClient> &ptr) override;
            bool notifyClientChatClosed(const std::shared_ptr<ConnectedClient> &ptr) override;
            bool notifyTextMessage(ChatMessageMode mode, const std::shared_ptr<ConnectedClient> &sender, uint64_t targetId, ChannelId channel_id, const std::chrono::system_clock::time_point&, const std::string &textMessage) override;

            bool notifyServerGroupClientAdd(std::optional<ts::command_builder> &anOptional,
                                            const std::shared_ptr<ConnectedClient> &ptr,
                                            const std::shared_ptr<ConnectedClient> &sharedPtr,
                                            const GroupId &id) override;

            bool notifyServerGroupClientRemove(std::optional<ts::command_builder> &anOptional,
                                               const std::shared_ptr<ConnectedClient> &ptr,
                                               const std::shared_ptr<ConnectedClient> &sharedPtr,
                                               const GroupId &id) override;

            bool notifyClientChannelGroupChanged(std::optional<ts::command_builder> &anOptional,
                                                 const std::shared_ptr<ConnectedClient> &ptr,
                                                 const std::shared_ptr<ConnectedClient> &sharedPtr, const ChannelId &id,
                                                 const ChannelId &channelId, const GroupId &groupId) override;

            bool notifyChannelMoved(const std::shared_ptr<BasicChannel> &channel, ChannelId order, const std::shared_ptr<ConnectedClient> &invoker) override;
            bool notifyChannelCreate(const std::shared_ptr<BasicChannel> &channel, ChannelId orderId,
                                                     const std::shared_ptr<ConnectedClient> &invoker) override;
            bool notifyChannelDescriptionChanged(std::shared_ptr<BasicChannel> channel) override;
            bool notifyChannelPasswordChanged(std::shared_ptr<BasicChannel> ) override;

            bool notifyChannelEdited(const std::shared_ptr<BasicChannel> &ptr, const std::vector<property::ChannelProperties> &vector, const std::shared_ptr<ConnectedClient> &sharedPtr, bool b) override;

            bool notifyChannelDeleted(const std::deque<ChannelId> &deque, const std::shared_ptr<ConnectedClient> &ptr) override;

            bool notifyMusicQueueAdd(const std::shared_ptr<MusicClient> &bot, const std::shared_ptr<ts::music::SongInfo> &entry, int index, const std::shared_ptr<ConnectedClient> &invoker) override;
            bool notifyMusicQueueRemove(const std::shared_ptr<MusicClient> &bot, const std::deque<std::shared_ptr<music::SongInfo>> &entry, const std::shared_ptr<ConnectedClient> &invoker) override;
            bool notifyMusicQueueOrderChange(const std::shared_ptr<MusicClient> &bot, const std::shared_ptr<ts::music::SongInfo> &entry, int order, const std::shared_ptr<ConnectedClient> &invoker) override;

            bool notifyMusicPlayerSongChange(const std::shared_ptr<MusicClient> &bot, const std::shared_ptr<music::SongInfo> &newEntry) override;

            bool notifyClientEnterView(const std::shared_ptr<ConnectedClient> &client, const std::shared_ptr<ConnectedClient> &invoker, const std::string &string, const std::shared_ptr<BasicChannel> &to, ViewReasonId reasonId,
                                       const std::shared_ptr<BasicChannel> &from, bool) override;

            bool notifyClientEnterView(const std::deque<std::shared_ptr<ConnectedClient>> &deque, const ViewReasonSystemT &t) override;

            bool notifyClientMoved(const std::shared_ptr<ConnectedClient> &client, const std::shared_ptr<BasicChannel> &target_channel, ViewReasonId reason, std::string msg, std::shared_ptr<ConnectedClient> invoker, bool lock_channel_tree) override;

            bool notifyClientLeftView(const std::shared_ptr<ConnectedClient> &client, const std::shared_ptr<BasicChannel> &target_channel, ViewReasonId reasonId, const std::string &reasonMessage, std::shared_ptr<ConnectedClient> invoker,
                                      bool lock_channel_tree) override;

            bool notifyClientLeftView(const std::deque<std::shared_ptr<ConnectedClient>> &deque, const std::string &string, bool b, const ViewReasonServerLeftT &t) override;

            bool notifyClientLeftViewKicked(const std::shared_ptr<ConnectedClient> &client, const std::shared_ptr<BasicChannel> &target_channel, const std::string &message, std::shared_ptr<ConnectedClient> invoker, bool lock_channel_tree) override;

            bool notifyClientLeftViewBanned(const std::shared_ptr<ConnectedClient> &client, const std::string &message, std::shared_ptr<ConnectedClient> invoker, size_t length, bool lock_channel_tree) override;

        private:
            command_result handleCommandExit(Command&);
            command_result handleCommandLogin(Command&);
            command_result handleCommandLogout(Command&);
            command_result handleCommandServerSelect(Command &);
            command_result handleCommandServerInfo(Command&);
            command_result handleCommandChannelList(Command&);

            command_result handleCommandServerList(Command&);
            command_result handleCommandServerCreate(Command&);
            command_result handleCommandServerDelete(Command&);
            command_result handleCommandServerStart(Command&);
            command_result handleCommandServerStop(Command&);

            command_result handleCommandInstanceInfo(Command&);
            command_result handleCommandInstanceEdit(Command&);

            command_result handleCommandBindingList(Command&);

            command_result handleCommandHostInfo(Command&);

            command_result handleCommandGlobalMessage(Command&);

            command_result handleCommandServerIdGetByPort(Command&);

            command_result handleCommandServerSnapshotDeployNew(const command_parser&);
            command_result handleCommandServerSnapshotCreate(Command&);
            command_result handleCommandServerProcessStop(Command&);

            command_result handleCommandServerNotifyRegister(Command&);
            command_result handleCommandServerNotifyList(Command&);
            command_result handleCommandServerNotifyUnregister(Command&);

            command_result handleCommandSetCompressionMode(Command&);
    };

    class QueryClientCommandHandler : public ts::server::ServerCommandHandler {
        public:
            explicit QueryClientCommandHandler(const std::shared_ptr<QueryClient>& /* client */);

        protected:
            bool handle_command(const std::string_view &) override;

        private:
            std::weak_ptr<QueryClient> client_ref;
    };
}