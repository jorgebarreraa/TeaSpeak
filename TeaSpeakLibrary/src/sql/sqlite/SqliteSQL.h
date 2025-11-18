#pragma once

#include "../SqlQuery.h"
namespace sql {
    namespace sqlite {
        class SqliteCommand : public CommandData {
            public:
                std::shared_ptr<sqlite3_stmt> stmt = nullptr;
        };

        class SqliteManager : public SqlManager {
            public:
                SqliteManager();
                virtual ~SqliteManager();

                result connect(const std::string &string) override;
                bool connected() override;
                result disconnect() override;

                sqlite3* getDatabase() { return this->database; }
            protected:
                std::shared_ptr<CommandData> copyCommandData(std::shared_ptr<CommandData> ptr) override;
                std::shared_ptr<CommandData> allocateCommandData() override;
                result executeCommand(std::shared_ptr<CommandData> ptr) override;
                result queryCommand(std::shared_ptr<CommandData> ptr, const QueryCallback&fn) override;

            private:
                std::shared_ptr<sqlite3_stmt> allocateStatement(const std::string&);
                sqlite3* database = nullptr;
        };
    }
}