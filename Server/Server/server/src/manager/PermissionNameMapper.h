#pragma once

#include <string>
#include "PermissionManager.h"
#include "Definitions.h"

namespace ts {
    namespace permission {
        class PermissionNameMapper {
            public:
                struct Type {
                    enum value {
                        MIN,
                        TS3 = MIN,
                        TEAWEB,
                        TEACLIENT,
                        QUERY,
                        MAX
                    };

                    inline static value from_client_type(const server::ClientType& client) {
                        if(client == server::ClientType::CLIENT_TEAMSPEAK)
                            return value::TS3;

                        if(client == server::ClientType::CLIENT_TEASPEAK)
                            return value::TEACLIENT;

                        if(client == server::ClientType::CLIENT_WEB)
                            return value::TEAWEB;

                        return value::QUERY;
                    }
                };

                PermissionNameMapper();
                virtual ~PermissionNameMapper();

                bool initialize(const std::string& file, std::string& error);

                std::string permission_name(const Type::value& /* type */, const permission::PermissionType& /* permission */) const;
                inline std::string permission_name(const server::ClientType& type, const permission::PermissionType& permission) const {
                    return this->permission_name(Type::from_client_type(type), permission);
                }

                std::string permission_name_grant(const Type::value& /* type */, const permission::PermissionType& /* permission */) const;
                inline std::string permission_name_grant(const server::ClientType& type, const permission::PermissionType& permission) const {
                    return this->permission_name_grant(Type::from_client_type(type), permission);
                }
            private:
                struct PermissionMap {
                    std::string mapped_name;
                    std::string grant_name;
                };
                std::array<
                    std::array<PermissionMap, permission::PermissionType::permission_id_max>,
                    Type::value::MAX
                > mapping;
        };
    }
}