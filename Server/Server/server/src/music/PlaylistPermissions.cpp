//
// Created by wolverindev on 30.01.20.
//

#include "./PlaylistPermissions.h"
#include "../client/ConnectedClient.h"

using namespace ts;
using namespace ts::music;


PlaylistPermissions::PlaylistPermissions(std::shared_ptr<permission::v2::PermissionManager> permissions) : _permissions{std::move(permissions)} {}

permission::v2::PermissionFlaggedValue PlaylistPermissions::calculate_client_specific_permissions(ts::permission::PermissionType permission, const std::shared_ptr<server::ConnectedClient>& client) {
    auto val = this->_permissions->channel_permission(permission, client->getClientDatabaseId());
    if(val.flags.value_set) {
        return {val.values.value, true};
    }
    return client->calculate_permission(permission, 0);
}

permission::PermissionType PlaylistPermissions::client_has_permissions(
        const std::shared_ptr<server::ConnectedClient> &client,
        ts::permission::PermissionType needed_permission,
        ts::permission::PermissionType granted_permission,
        uint8_t flags
) {
    if(this->is_playlist_owner(client->getClientDatabaseId())) {
        return permission::ok;
    }

    const auto client_permission = this->calculate_client_specific_permissions(granted_permission, client);
    auto playlist_permission = this->permission_manager()->permission_value_flagged(needed_permission);

    if((flags & do_no_require_granted) == 0) {
        playlist_permission.clear_flag_on_zero();
    } else {
        playlist_permission.zero_if_unset();
    }

    return permission::v2::permission_granted(playlist_permission, client_permission) ? permission::ok : granted_permission;
}