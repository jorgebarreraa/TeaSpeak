#pragma once

#include "Definitions.h"
#include "command_handler.h"

namespace ts {
    namespace cconstants {

        typedef command_handler::field<tl("return_code"), std::string> return_code;
        typedef command_handler::field<tl("reasonmsg"), std::string> reasonmsg;

        typedef command_handler::field<tl("sid"), ServerId> server_id;

        typedef command_handler::field<tl("clid"), ClientId> client_id;
        typedef command_handler::field<tl("cldbid"), ClientDbId> client_database_id;

        typedef command_handler::field<tl("cid"), ChannelId> channel_id;
        typedef command_handler::field<tl("cpid"), ChannelId> channel_parent_id;

        typedef command_handler::field<tl("cgid"), GroupId> channel_group_id;
        typedef command_handler::field<tl("sgid"), GroupId> server_group_id;

        //FIXME
        /* typedef descriptor::field<tl("permid"), permission::PermissionType> permission_id;
        typedef descriptor::field<tl("permsid"), std::string> permission_name;
        typedef descriptor::field<tl("permvalue"), permission::PermissionValue> permission_value;
        */
    }
}