#pragma once

#include <chrono>
#include <Definitions.h>
#include <vector>

namespace ts {
    namespace server {
        class VirtualServer;
    }


    namespace letter {
        struct LetterHeader {
            LetterId id;
            ClientUid sender;
            ClientUid receiver;
            std::chrono::time_point<std::chrono::system_clock> created;
            std::string subject;
            bool read;
        };

        struct Letter : public LetterHeader {
            std::string message;
        };

        class LetterManager {
            public:
                LetterManager(server::VirtualServer*);
                ~LetterManager();


                size_t unread_letter_count(const ClientUid&);
                std::vector<std::shared_ptr<LetterHeader>> avariableLetters(ClientUid);
                std::shared_ptr<Letter> getFullLetter(LetterId);

                void updateReadFlag(LetterId, bool);
                void deleteLetter(LetterId);

                void createLetter(const ClientUid& sender, const ClientUid& reciver, const std::string& subject, const std::string& message);
            private:
                server::VirtualServer* server;
        };
    }
}