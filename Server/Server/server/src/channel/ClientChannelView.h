#pragma once

#include <channel/TreeView.h>
#include <BasicChannel.h>

namespace ts {
    namespace server {
        class ConnectedClient;
        struct CalculateCache;
    }

    struct ViewEntry : public TreeEntry {
        public:
            ViewEntry(const std::shared_ptr<BasicChannel>&, bool /* editable */ = false);
            ~ViewEntry();

            inline std::shared_ptr<BasicChannel> channel() { return this->handle.lock(); }
            ChannelId channelId() const override;
            ChannelId parentId() const;
            ChannelId previousChannelId() const override;
            void setPreviousChannelId(ChannelId id) override;
            void setParentChannelId(ChannelId id) override;

            bool subscribed = false;
            bool editable = false;
            permission::PermissionType join_permission_error = permission::unknown; /* used within notify text message */
            ChannelId previous_channel = 0;
            uint16_t join_state_id = 0; /* the calculation id for the flag joinable. If this does not match with the join_state_id within the client the flag needs to be recalculated  */
            std::weak_ptr<BasicChannel> handle;
            std::chrono::system_clock::time_point view_timestamp;
        private:
            ChannelId cached_channel_id = 0;
            ChannelId cached_parent_id = 0;
    };

    class ClientChannelView : private TreeView {
        public:
            enum ChannelAction {
                NOTHING,
                ENTER_VIEW,
                DELETE_VIEW,
                REORDER,
                MOVE
            };

            explicit ClientChannelView(server::ConnectedClient*);
            ~ClientChannelView() override;

            inline size_t count_channels() const { return this->entry_count(); }
            std::deque<std::shared_ptr<BasicChannel>> channels(const std::shared_ptr<BasicChannel>& /* head */ = nullptr, int deep = -1);
            bool channel_visible(const std::shared_ptr<BasicChannel>& /* entry */, const std::shared_ptr<BasicChannel>& /* head */ = nullptr, int deep = -1);
            std::shared_ptr<ViewEntry> find_channel(ChannelId /* channel id */);
            std::shared_ptr<ViewEntry> find_channel(const std::shared_ptr<BasicChannel>& /* channel */);

            /* add channel tree with siblings */
            std::deque<std::shared_ptr<ViewEntry>> insert_channels(
                    std::shared_ptr<TreeView::LinkedTreeEntry> /* head */,
                    bool test_permissions,
                    bool first_only
            );

            /* shows the specific channel and their parents */
            std::deque<std::shared_ptr<ViewEntry>> show_channel(
                    std::shared_ptr<TreeView::LinkedTreeEntry> /* channel */,
                            bool& /* success */
            );

            /* remove invalid channel */
            std::deque<std::shared_ptr<ViewEntry>> test_channel(
                    std::shared_ptr<TreeView::LinkedTreeEntry> /* old channel */,
                    std::shared_ptr<TreeView::LinkedTreeEntry> /* new channel */
            );

            /* [{ add := true | delete := false, channel}] */
            std::deque<std::pair<bool, std::shared_ptr<ViewEntry>>> update_channel(
                    std::shared_ptr<TreeView::LinkedTreeEntry> /* channel */,
                    std::shared_ptr<TreeView::LinkedTreeEntry> /* own channel */
            );

            /* [{ add := true | delete := false, channel}] */
            std::deque<std::pair<bool, std::shared_ptr<ViewEntry>>> update_channel_path(
                    std::shared_ptr<TreeView::LinkedTreeEntry> /* channel */,
                    std::shared_ptr<TreeView::LinkedTreeEntry> /* own channel */,
                    ssize_t length = -1
            );

            /* triggered on channel create */
            std::shared_ptr<ViewEntry> add_channel(const std::shared_ptr<TreeView::LinkedTreeEntry>& /* channel */);
            /* triggered on channel delete */
            bool remove_channel(ChannelId channelId);


            std::deque<std::pair<ChannelAction, std::shared_ptr<ViewEntry>>> change_order(
                    const std::shared_ptr<LinkedTreeEntry> &/* channel */,
                    const std::shared_ptr<LinkedTreeEntry> /* parent */&,
                    const std::shared_ptr<LinkedTreeEntry> /* previous */&
            );

            std::deque<ChannelId> delete_channel_root(const std::shared_ptr<BasicChannel>& /* channel */);

            void print();
            void reset();
        private:
            ServerId getServerId();
            server::ConnectedClient* owner;
    };
}