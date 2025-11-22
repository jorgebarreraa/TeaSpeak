#pragma once

#include <array>
#include <string>
#include <tomcrypt.h>
#include "./Packet.h"
#undef byte /* the macro byte gets defined by tomcrypt_macros. We have to undefine it */

namespace ts::connection {
    class CryptHandler {
        public:
            typedef std::array<uint8_t, 16> key_t;
            typedef std::array<uint8_t, 16> nonce_t;
            CryptHandler();
            ~CryptHandler();

            void reset();

            bool setupSharedSecret(const std::string& /* alpha */, const std::string& /* beta */, ecc_key* /* remote_public_key */, ecc_key* /* own_private_key */, std::string &/* error */);
            bool setupSharedSecret(const std::string& /* alpha */, const std::string& /* beta */, const std::string& /* shared_key */, std::string &/* error */);

            bool setupSharedSecretNew(const std::string& alpha, const std::string& beta, const char privateKey[32], const char publicKey[32]);

            bool encrypt(
                    const void* /* header */, size_t /* header length */,
                    void* /* payload */, size_t /* payload length */,
                    void* /* mac */, /* mac must be 8 bytes long! */
                    const key_t& /* key */, const nonce_t& /* nonce */,
                    std::string& /* error */);

            bool decrypt(
                    const void* /* header */, size_t /* header length */,
                    void* /* payload */, size_t /* payload length */,
                    const void* /* mac */, /* mac must be 8 bytes long! */
                    const key_t& /* key */, const nonce_t& /* nonce */,
                    std::string& /* error */) const;

            bool generate_key_nonce(bool /* to server */, uint8_t /* packet type */, uint16_t /* packet id */, uint16_t /* generation */, key_t& /* key */, nonce_t& /* nonce */);
            bool verify_encryption(const pipes::buffer_view& /* data */, uint16_t /* packet id */, uint16_t /* generation */);

            inline void write_default_mac(void* buffer) {
                memcpy(buffer, this->current_mac, 8);
            }

            [[nodiscard]] inline bool encryption_initialized() const { return !this->encryption_initialized_; }

            static constexpr key_t kDefaultKey{'c', ':', '\\', 'w', 'i', 'n', 'd', 'o', 'w', 's', '\\', 's', 'y', 's', 't', 'e'}; //c:\windows\syste
            static constexpr nonce_t kDefaultNonce{'m', '\\', 'f', 'i', 'r', 'e', 'w', 'a', 'l', 'l', '3', '2', '.', 'c', 'p', 'l'}; //m\firewall32.cpl
        private:
            static constexpr char default_mac[8] = {'T', 'S', '3', 'I', 'N', 'I', 'T', '1'}; //TS3INIT1

            struct KeyCache {
                uint16_t generation = 0xFFEF;
                union _key_nonce {
                    struct {
                        uint8_t key[16];
                        uint8_t nonce[16];
                    };
                    uint8_t value[32];
                } key_nonce;
            };

            bool encryption_initialized_{false};
            int cipher_code{-1};

            /* for the old protocol SHA1 length for the new 64 bytes */
            uint8_t iv_struct[64];
            uint8_t iv_struct_length{0};

            uint8_t current_mac[8]{};

            std::mutex cache_key_lock{};
            std::array<KeyCache, /* protocol::PACKET_MAX */ 0x08> cache_key_client{};
            std::array<KeyCache, /* protocol::PACKET_MAX */ 0x08> cache_key_server{};

            static_assert(sizeof(current_mac) == sizeof(default_mac), "invalid mac");
            static_assert(sizeof(iv_struct) == 64, "invalid iv struct");
    };
}