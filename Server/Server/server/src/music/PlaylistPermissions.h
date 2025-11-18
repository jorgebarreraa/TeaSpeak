#pragma once

#include <PermissionManager.h>

namespace ts::server {
    class VirtualServer;
    class ConnectedClient;
}

namespace ts::music {
    class PlaylistPermissions {
        public:
            enum permission_flags {
                ignore_playlist_owner,
                do_no_require_granted
            };
            explicit PlaylistPermissions(std::shared_ptr<permission::v2::PermissionManager> permissions);

            [[nodiscard]] inline const std::shared_ptr<permission::v2::PermissionManager>& permission_manager() const { return this->_permissions; }

            /* returns permission::ok if client has permissions */
            permission::PermissionType client_has_permissions(
                    const std::shared_ptr<server::ConnectedClient>& /* client */,
                    permission::PermissionType /* needed_permission */,
                    permission::PermissionType /* granted_permission */,
                    uint8_t /* ignore playlist owner */ = 0
            );

            permission::v2::PermissionFlaggedValue calculate_client_specific_permissions(
                    permission::PermissionType /* permission */,
                    const std::shared_ptr<server::ConnectedClient>& /* client */
            );

        protected:
            const std::shared_ptr<permission::v2::PermissionManager> _permissions;

            [[nodiscard]] virtual bool is_playlist_owner(ClientDbId /* database id */) const = 0;
    };
}