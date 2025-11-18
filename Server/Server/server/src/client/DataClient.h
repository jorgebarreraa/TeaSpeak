#pragma once

#include "Variable.h"
#include <string>
#include <utility>
#include <query/Command.h>
#include <BasicChannel.h>
#include <Error.h>
#include <netinet/in.h>
#include "src/VirtualServer.h"
#include "../Group.h"

#define DEBUG_PERMISSION

namespace ts {
    namespace music {
        class MusicBotManager;
    }
    namespace server {
        class VirtualServer;

        namespace groups {
            class ServerGroup;
            class ChannelGroup;
            typedef void TemporaryAssignmentsLock;
        }

        class DataClient {
                friend class VirtualServer;
                friend class QueryServer;
                friend class music::MusicBotManager;
            public:
                DataClient(sql::SqlManager*, const std::shared_ptr<VirtualServer>&);
                virtual ~DataClient();

                inline PropertyWrapper properties() { return PropertyWrapper{this->_properties}; }
                inline const PropertyWrapper properties() const { return PropertyWrapper{this->_properties}; }
                [[nodiscard]] inline auto permissions(){ return this->clientPermissions; }

                /* main permission calculate function */
                /**
                 * Calculate the given permissions.
                 * This method can be called from everywhere without any locking needed.
                 * @param channel
                 * @param granted
                 * @return
                 */
                permission::v2::PermissionFlaggedValue calculate_permission(
                        permission::PermissionType,
                        ChannelId channel,
                        bool granted = false
                );

                /**
                 * Calculate the given permissions.
                 * This method can be called from everywhere without any locking needed.
                 * @param channel
                 * @param granted
                 * @return
                 */
                std::vector<std::pair<permission::PermissionType, permission::v2::PermissionFlaggedValue>> calculate_permissions(
                        const std::deque<permission::PermissionType>&,
                        ChannelId channel,
                        bool granted = false
                );

                virtual std::vector<std::shared_ptr<groups::ServerGroup>> assignedServerGroups();
                virtual std::shared_ptr<groups::ChannelGroup> assignedChannelGroup(std::shared_ptr<BasicChannel> &);
                virtual bool serverGroupAssigned(const std::shared_ptr<groups::ServerGroup> &);
                virtual bool channelGroupAssigned(const std::shared_ptr<groups::ChannelGroup> &, const std::shared_ptr<BasicChannel> &);

                virtual std::string getDisplayName() { return this->properties()[property::CLIENT_NICKNAME]; }
                virtual std::string getLoginName() { return this->properties()[property::CLIENT_LOGIN_NAME]; }
                virtual void setDisplayName(std::string displayName) { this->properties()[property::CLIENT_NICKNAME] = displayName; }

                [[nodiscard]] inline std::shared_ptr<VirtualServer> getServer() { return this->server; }
                [[nodiscard]] inline ServerId  getServerId() {
                    auto server_ref = this->getServer();
                    return server_ref ? server_ref->getServerId() : (ServerId) 0;
                }

                virtual ClientType getExternalType(){
                    uint8_t type = this->properties()[property::CLIENT_TYPE];
                    return (ClientType) type;
                }

                virtual ClientType getType(){
                    uint8_t type = this->properties()[property::CLIENT_TYPE_EXACT];
                    return (ClientType) type;
                }

                virtual std::string getUid() { return this->properties()[property::CLIENT_UNIQUE_IDENTIFIER]; }
                virtual std::string getAvatarId();
                virtual ClientDbId getClientDatabaseId() { return this->properties()[property::CLIENT_DATABASE_ID]; }


                virtual bool loadDataForCurrentServer();
            protected:
                sql::SqlManager* sql;
                std::shared_ptr<VirtualServer> server;

                std::shared_ptr<permission::v2::PermissionManager> clientPermissions = nullptr;
                std::shared_ptr<PropertyManager> _properties;

                std::shared_ptr<BasicChannel> currentChannel = nullptr;
                std::shared_ptr<groups::TemporaryAssignmentsLock> temporary_assignments_lock{};
        };
    }
}