#pragma once

#include <sql/SqlQuery.h>

namespace ts {
    namespace server {
        class SqlDataManager {
            public:
                SqlDataManager();
                virtual ~SqlDataManager();

                [[nodiscard]] inline int get_database_version() const { return this->_database_version; }
                [[nodiscard]] inline int get_permissions_version() const { return this->_permissions_version; }
                bool initialize(std::string&);
                void finalize();

                sql::SqlManager* sql() { return this->manager; }
            private:
                sql::SqlManager* manager = nullptr;
                int _database_version = -1;
                int _permissions_version = -1;
                bool database_version_present = false;
                bool permissions_version_present = false;

                bool detect_versions();
                bool change_database_version(int);
                bool change_permission_version(int);

                bool update_database(std::string& /* errror */);
                bool update_permissions(std::string& /* errror */);
        };
    }
}