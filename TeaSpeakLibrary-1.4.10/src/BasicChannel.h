#pragma once

#include <cstdint>
#include <cstdlib>
#include "misc/advanced_mutex.h"
#include "channel/TreeView.h"
#include "Definitions.h"
#include "Properties.h"
#include "PermissionManager.h"

namespace ts {

    class BasicChannel;
    class BasicChannelTree;

    namespace ChannelType {
        enum ChannelType {
            permanent,
            semipermanent,
            temporary
        };
    }

    struct BasicChannelEntry {
        BasicChannelEntry* previus{};
        std::shared_ptr<BasicChannel> current = nullptr;
        BasicChannelEntry* next{};

        BasicChannelEntry* children{};

        ~BasicChannelEntry() {
            this->current = nullptr;
        }
    };

    class BasicChannel : public TreeEntry {
        public:
            BasicChannel(ChannelId parentId, ChannelId channelId);
            BasicChannel(std::shared_ptr<BasicChannel> parent, ChannelId channelId);

            virtual ~BasicChannel();

            bool hasParent(){ return !!this->parent(); }
            std::shared_ptr<BasicChannel> parent();

            inline std::string name(){ return properties()[property::CHANNEL_NAME]; }
            inline ChannelId channelOrder(){ return this->previousChannelId(); }

            inline PropertyWrapper properties() { return PropertyWrapper{this->_properties}; }
            inline const PropertyWrapper properties() const { return PropertyWrapper{this->_properties}; }

            ChannelType::ChannelType channelType();
            void setChannelType(ChannelType::ChannelType);
            void updateChannelType(std::vector<property::ChannelProperties>& /* updates */, ChannelType::ChannelType /* target type */);

            [[nodiscard]] bool verify_password(const std::optional<std::string>&, bool /* password already hashed */);

            bool defaultChannel() { return (*this->_properties)[property::CHANNEL_FLAG_DEFAULT]; }
            uint64_t empty_seconds();

            inline std::chrono::system_clock::time_point createdTimestamp() {
                return std::chrono::system_clock::time_point() + std::chrono::milliseconds(
                        this->properties()[property::CHANNEL_CREATED_AT].as_unchecked<int64_t>());
            }

            ts_always_inline bool permission_require_property_update(const permission::PermissionType& permission) {
                return permission == permission::i_icon_id || permission == permission::i_client_needed_talk_power || permission == permission::i_channel_needed_view_power;
            }
            std::vector<property::ChannelProperties> update_properties_from_permissions(bool& /* need view update */);

            inline bool permission_granted(const permission::PermissionType& permission, const permission::v2::PermissionFlaggedValue& granted_value, bool require_granted_value) {
                auto permission_manager = this->permissions(); /* copy the manager */
                assert(permission_manager);
                const auto data = permission_manager->permission_value_flagged(permission);
                return BasicChannel::permission_granted(data, granted_value, require_granted_value);
            }

            ts_always_inline bool talk_power_granted(const permission::v2::PermissionFlaggedValue& granted_value) {
                return this->permission_granted(permission::i_client_needed_talk_power, granted_value, false);
            }

            ts_always_inline std::shared_ptr<permission::v2::PermissionManager> permissions(){ return this->_permissions; }
            virtual void setPermissionManager(const std::shared_ptr<permission::v2::PermissionManager>&);
            virtual void setProperties(const std::shared_ptr<PropertyManager>&);
        private:
            ts_always_inline
            static bool permission_granted(const permission::v2::PermissionFlaggedValue& channel_permission_value, const permission::v2::PermissionFlaggedValue& granted_value, bool require_granted_value) {
                if(!channel_permission_value.has_value || channel_permission_value.value == 0) {
                    return granted_value.has_value ? granted_value.has_power() : !require_granted_value;
                }
                if(channel_permission_value.value == -1) {
                    return granted_value.value == -1;
                }
                return granted_value.value >= channel_permission_value.value;
            }

        public:
            [[nodiscard]] ChannelId channelId() const override;
            [[nodiscard]] ChannelId previousChannelId() const override;
            void setPreviousChannelId(ChannelId id) override;
            void setParentChannelId(ChannelId id) override;
            void setLinkedHandle(const std::weak_ptr<TreeView::LinkedTreeEntry> &) override;
        protected:
            std::weak_ptr<TreeView::LinkedTreeEntry> _link;
            std::shared_ptr<PropertyManager> _properties;
            std::shared_ptr<permission::v2::PermissionManager> _permissions;

            permission::v2::PermissionFlaggedValue last_view_power{0, false};

            ChannelId _channel_order = 0;
            ChannelId _channel_id = 0;
    };

    class BasicChannelTree : public TreeView {
        public:
            BasicChannelTree();
            virtual ~BasicChannelTree();

            size_t channel_count() { return this->entry_count(); }

            std::shared_ptr<LinkedTreeEntry> findLinkedChannel(ChannelId channelId);
            std::shared_ptr<BasicChannel> findChannel(ChannelId channelId);
            std::shared_ptr<BasicChannel> findChannel(ChannelId channelId, std::deque<std::shared_ptr<BasicChannel>> avariable);
            std::shared_ptr<BasicChannel> findChannel(const std::string &name, const std::shared_ptr<BasicChannel> &layer);
            std::shared_ptr<BasicChannel> findChannelByPath(const std::string& path);
            //std::deque<std::shared_ptr<BasicChannel>> topChannels();
            std::deque<std::shared_ptr<BasicChannel>> channels(const std::shared_ptr<BasicChannel> &root = nullptr, int deep = -1);

            virtual std::shared_ptr<BasicChannel> createChannel(ChannelId parentId, ChannelId orderId, const std::string &name);
            virtual std::deque<std::shared_ptr<ts::BasicChannel>> delete_channel_root(const std::shared_ptr<BasicChannel> &);

            inline bool change_order(const std::shared_ptr<BasicChannel>& channel, ChannelId channelId) {
                return this->move_entry(channel, channel->parent(), this->find_entry(channelId));
            }
            inline bool move_channel(const std::shared_ptr<BasicChannel>& channel, const std::shared_ptr<BasicChannel>& parent, const std::shared_ptr<BasicChannel>& order) {
                return this->move_entry(channel, parent, order);
            }

            bool setDefaultChannel(const std::shared_ptr<BasicChannel> &);
            std::shared_ptr<BasicChannel> getDefaultChannel();

            void printChannelTree(std::function<void(std::string)> = [](std::string msg) { std::cout << msg << std::endl; });
        protected:
            virtual ChannelId generateChannelId();
            virtual void on_channel_entry_deleted(const std::shared_ptr<BasicChannel> &);
            virtual std::shared_ptr<BasicChannel> allocateChannel(const std::shared_ptr<BasicChannel> &parent, ChannelId channelId);
    };
}

DEFINE_TRANSFORMS(ts::ChannelType::ChannelType, uint8_t);