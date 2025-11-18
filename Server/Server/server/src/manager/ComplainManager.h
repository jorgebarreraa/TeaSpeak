#pragma once

#include <sql/SqlQuery.h>
#include <Definitions.h>
#include "Variable.h"

namespace ts {
    namespace  server {
        class VirtualServer;
    }
    struct ComplainEntry;
}

namespace ts {
    struct ComplainEntry {
        ClientDbId target;
        ClientDbId invoker;
        std::string reason;
        std::chrono::time_point<std::chrono::system_clock> created;
    };

    class ComplainManager {
        public:
            ComplainManager(server::VirtualServer*);
            ~ComplainManager();

            bool loadComplains();

            std::deque<std::shared_ptr<ComplainEntry>> complains(){ return this->entries; }
            std::deque<std::shared_ptr<ComplainEntry>> findComplainsFromReporter(ClientDbId);
            std::deque<std::shared_ptr<ComplainEntry>> findComplainsFromTarget(ClientDbId);

            bool deleteComplain(std::shared_ptr<ComplainEntry>);
            bool deleteComplainsFromTarget(ClientDbId);
            bool deleteComplainsFromReporter(ClientDbId);

            std::shared_ptr<ComplainEntry> createComplain(ClientDbId target, ClientDbId reporter, std::string msg);
        private:
            server::VirtualServer* server;
            threads::Mutex entryLock;
            std::deque<std::shared_ptr<ComplainEntry>> entries;
    };
}