#pragma once

#include <array>
#include <string>
#include "Packet.h"
#include <tomcrypt.h>
#undef byte /* the macro byte gets defined by tomcrypt_macros. We have to undefine it */

namespace ts {
    namespace connection {
        class CryptHandler {
            enum Methode {
                TEAMSPEAK_3_1,
                TEAMSPEAK_3
            };
            struct KeyCache {
                uint16_t generation = 0xFFEF;
                union {
                    struct {
                        uint8_t key[16];
                        uint8_t nonce[16];
                    };
                    uint8_t key_nonce[32];
                };
            };
            public:
                typedef std::array<uint8_t, 16> key_t;
                typedef std::array<uint8_t, 16> nonce_t;
                CryptHandler();
                ~CryptHandler();

                void reset();

                //TeamSpeak old
                bool setupSharedSecret(const std::string& alpha, const std::string& beta, ecc_key* publicKey, ecc_key* ownKey, std::string &error);
                bool setupSharedSecret(const std::string& alpha, const std::string& beta, const std::string& sharedKey, std::string &error);

                //TeamSpeak new
                bool setupSharedSecretNew(const std::string& alpha, const std::string& beta, const char privateKey[32], const char publicKey[32]);

                /* mac must be 8 bytes long! */
                bool encrypt(
                        const void* /* header */, size_t /* header length */,
                        void* /* payload */, size_t /* payload length */,
                        void* /* mac */,
                        const key_t& /* key */, const nonce_t& /* nonce */,
                        std::string& /* error */);

                /* mac must be 8 bytes long! */
                bool decrypt(
                        const void* /* header */, size_t /* header length */,
                        void* /* payload */, size_t /* payload length */,
                        const void* /* mac */,
                        const key_t& /* key */, const nonce_t& /* nonce */,
                        std::string& /* error */);

                bool generate_key_nonce(bool /* to server */, uint8_t /* packet type */, uint16_t /* packet id */, uint16_t /* generation */, key_t& /* key */, nonce_t& /* nonce */);
                bool verify_encryption(const pipes::buffer_view& data, uint16_t packet_id, uint16_t generation);

                inline void write_default_mac(void* buffer) {
                    memcpy(buffer, this->current_mac, 8);
                }

                static constexpr key_t default_key{'c', ':', '\\', 'w', 'i', 'n', 'd', 'o', 'w', 's', '\\', 's', 'y', 's', 't', 'e'}; //c:\windows\syste
                static constexpr nonce_t default_nonce{'m', '\\', 'f', 'i', 'r', 'e', 'w', 'a', 'l', 'l', '3', '2', '.', 'c', 'p', 'l'}; //m\firewall32.cpl
            private:
                static constexpr char default_mac[8] = {'T', 'S', '3', 'I', 'N', 'I', 'T', '1'}; //TS3INIT1


                bool generate_key_nonce(protocol::BasicPacket* packet, bool use_default, uint8_t(&)[16] /* key */, uint8_t(&)[16] /* nonce */);


                //The default key and nonce
                bool useDefaultChipherKeyNonce = true;

                /* for the old protocol SHA1 length for the new 64 bytes */
                uint8_t iv_struct[64];
                uint8_t iv_struct_length = 0;

                uint8_t current_mac[8];

                std::mutex cache_key_lock;
                std::array<KeyCache, protocol::PACKET_MAX> cache_key_client;
                std::array<KeyCache, protocol::PACKET_MAX> cache_key_server;

                static_assert(sizeof(current_mac) == sizeof(default_mac), "invalid mac");
                static_assert(sizeof(iv_struct) == 64, "invalid iv struct");
        };
    }
}