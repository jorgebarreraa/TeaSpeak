//
// Created by WolverinDEV on 03/03/2020.
//

#include "Group.h"
#include "./GroupManager.h"

using namespace ts::server::groups;

Group::Group(ServerId sid, ts::GroupId id, ts::server::groups::GroupType type, std::string name,
             std::shared_ptr<permission::v2::PermissionManager> permissions) : virtual_server_id_{sid}, group_id_{id}, type_{type}, name_{std::move(name)}, permissions_{std::move(permissions)} { }

void Group::set_permissions(const std::shared_ptr<permission::v2::PermissionManager> &permissions) {
    assert(permissions);
    this->permissions_ = permissions;
}

ServerGroup::ServerGroup(ServerId sid, GroupId id, GroupType type, std::string name,
                         std::shared_ptr<permission::v2::PermissionManager> permissions)
    : Group{sid, id, type, std::move(name), std::move(permissions)}
{

}

ChannelGroup::ChannelGroup(ServerId sid, GroupId id, GroupType type, std::string name,
                         std::shared_ptr<permission::v2::PermissionManager> permissions)
        : Group{sid, id, type, std::move(name), std::move(permissions)}
{

}