//#define NO_OPEN_SSL /* because we're lazy and dont want to build this lib extra for the TeaClient */
#define FIXEDINT_H_INCLUDED /* else it will be included by ge */

#include "misc/endianness.h"
#include <ed25519/ed25519.h>
#include <ed25519/ge.h>
#include <log/LogUtils.h>
#include "misc/memtracker.h"
#include "misc/digest.h"
#include "CryptHandler.h"
#include "../misc/sassert.h"

using namespace std;
using namespace ts;
using namespace ts::connection;
using namespace ts::protocol;


CryptHandler::CryptHandler() {
    memtrack::allocated<CryptHandler>(this);
}

CryptHandler::~CryptHandler() {
    memtrack::freed<CryptHandler>(this);
}

void CryptHandler::reset() {
    this->useDefaultChipherKeyNonce = true;
    this->iv_struct_length = 0;
    memset(this->iv_struct, 0, sizeof(this->iv_struct));
    memcpy(this->current_mac, CryptHandler::default_mac, sizeof(CryptHandler::default_mac));

    for(auto& cache : this->cache_key_client)
        cache.generation = 0xFFEF;
    for(auto& cache : this->cache_key_server)
        cache.generation = 0xFFEF;
}

#define SHARED_KEY_BUFFER_LENGTH (256)
bool CryptHandler::setupSharedSecret(const std::string& alpha, const std::string& beta, ecc_key *publicKey, ecc_key *ownKey, std::string &error) {
    size_t buffer_length = SHARED_KEY_BUFFER_LENGTH;
    uint8_t buffer[SHARED_KEY_BUFFER_LENGTH];
    int err;
    if((err = ecc_shared_secret(ownKey, publicKey, buffer, (unsigned long*) &buffer_length)) != CRYPT_OK){
        error = "Could not calculate shared secret. Message: " + string(error_to_string(err));
        return false;
    }

    auto result = this->setupSharedSecret(alpha, beta, string((const char*) buffer, buffer_length), error);
    return result;
}

bool CryptHandler::setupSharedSecret(const std::string& alpha, const std::string& beta, const std::string& sharedKey, std::string &error) {
    auto secret_hash = digest::sha1(sharedKey);
    assert(secret_hash.length() == SHA_DIGEST_LENGTH);

    uint8_t iv_buffer[SHA_DIGEST_LENGTH];
    memcpy(iv_buffer, alpha.data(), 10);
    memcpy(&iv_buffer[10], beta.data(), 10);

    for (int index = 0; index < SHA_DIGEST_LENGTH; index++) {
        iv_buffer[index] ^= (uint8_t) secret_hash[index];
    }

    {
        lock_guard lock(this->cache_key_lock);
        memcpy(this->iv_struct, iv_buffer, SHA_DIGEST_LENGTH);
        this->iv_struct_length = SHA_DIGEST_LENGTH;

        uint8_t mac_buffer[SHA_DIGEST_LENGTH];
        digest::sha1((const char*) iv_buffer, SHA_DIGEST_LENGTH, mac_buffer);
        memcpy(this->current_mac, mac_buffer, 8);

        this->useDefaultChipherKeyNonce = false;
    }

    return true;
}

inline void _fe_neg(fe h, const fe f) {
    int32_t f0 = f[0];
    int32_t f1 = f[1];
    int32_t f2 = f[2];
    int32_t f3 = f[3];
    int32_t f4 = f[4];
    int32_t f5 = f[5];
    int32_t f6 = f[6];
    int32_t f7 = f[7];
    int32_t f8 = f[8];
    int32_t f9 = f[9];
    int32_t h0 = -f0;
    int32_t h1 = -f1;
    int32_t h2 = -f2;
    int32_t h3 = -f3;
    int32_t h4 = -f4;
    int32_t h5 = -f5;
    int32_t h6 = -f6;
    int32_t h7 = -f7;
    int32_t h8 = -f8;
    int32_t h9 = -f9;

    h[0] = h0;
    h[1] = h1;
    h[2] = h2;
    h[3] = h3;
    h[4] = h4;
    h[5] = h5;
    h[6] = h6;
    h[7] = h7;
    h[8] = h8;
    h[9] = h9;
}

inline void keyMul(uint8_t(& target_buffer)[32], const uint8_t* publicKey /* compressed */, const uint8_t* privateKey /* uncompressed */, bool negate){
    ge_p3 keyA{};
    ge_p2 result{};

    ge_frombytes_negate_vartime(&keyA, publicKey);
    if(negate) {
        _fe_neg(*(fe*) &keyA.X, *(const fe*) &keyA.X); /* undo negate */
        _fe_neg(*(fe*) &keyA.T, *(const fe*) &keyA.T); /* undo negate */
    }
    ge_scalarmult_vartime(&result, privateKey, &keyA);

    ge_tobytes(target_buffer, &result);
}

bool CryptHandler::setupSharedSecretNew(const std::string &alpha, const std::string &beta, const char* privateKey /* uncompressed */, const char* publicKey /* compressed */) {
    if(alpha.length() != 10 || beta.length() != 54)
        return false;

    uint8_t shared[32];
    uint8_t shared_iv[64];

    ed25519_key_exchange(shared, (uint8_t*) publicKey, (uint8_t*) privateKey);
    keyMul(shared, reinterpret_cast<const uint8_t *>(publicKey), reinterpret_cast<const uint8_t *>(privateKey), true); //Remote key get negated
    digest::sha512((char*) shared, 32, shared_iv);

    auto xor_key = alpha + beta;
    for(int i = 0; i < 64; i++)
        shared_iv[i] ^= (uint8_t) xor_key[i];

    {
        lock_guard lock(this->cache_key_lock);
        memcpy(this->iv_struct, shared_iv, 64);
        this->iv_struct_length = 64;

        uint8_t mac_buffer[SHA_DIGEST_LENGTH];
        digest::sha1((char*) this->iv_struct, 64, mac_buffer);
        memcpy(this->current_mac, mac_buffer, 8);
        this->useDefaultChipherKeyNonce = false;
    }

    return true;
}

#define GENERATE_BUFFER_LENGTH (128)
bool CryptHandler::generate_key_nonce(
        bool to_server, /* its from the client to the server */
        uint8_t type,
        uint16_t packet_id,
        uint16_t generation,
        CryptHandler::key_t& key,
        CryptHandler::nonce_t& nonce
) {
    auto& key_cache_array = to_server ? this->cache_key_client : this->cache_key_server;
    if(type < 0 || type >= key_cache_array.max_size()) {
        logError(0, "Tried to generate a crypt key with invalid type ({})!", type);
        return false;
    }

    {
        std::lock_guard lock{this->cache_key_lock};
        auto& key_cache = key_cache_array[type];
        if(key_cache.generation != generation) {
            const size_t buffer_length = 6 + this->iv_struct_length;
            sassert(buffer_length < GENERATE_BUFFER_LENGTH);

            char buffer[GENERATE_BUFFER_LENGTH];
            memset(buffer, 0, buffer_length);

            if (to_server) {
                buffer[0] = 0x31;
            }  else {
                buffer[0] = 0x30;
            }
            buffer[1] = (char) (type & 0xF);

            le2be32(generation, buffer, 2);
            memcpy(&buffer[6], this->iv_struct, this->iv_struct_length);
            digest::sha256(buffer, buffer_length, key_cache.key_nonce);

            key_cache.generation = generation;
        }

        memcpy(key.data(), key_cache.key, 16);
        memcpy(nonce.data(), key_cache.nonce, 16);
    }

    //Xor the key
    key[0] ^= (uint8_t) ((packet_id >> 8) & 0xFFU);
    key[1] ^=(packet_id & 0xFFU);

    return true;
}

bool CryptHandler::verify_encryption(const pipes::buffer_view &packet, uint16_t packet_id, uint16_t generation) {
    int err;
    int success = false;

    key_t key{};
    nonce_t nonce{};
    if(!generate_key_nonce(true, (protocol::PacketType) (packet[12] & 0xF), packet_id, generation, key, nonce))
        return false;

    auto mac = packet.view(0, 8);
    auto header = packet.view(8, 5);
    auto data = packet.view(13);

    auto length = data.length();

    /* static shareable void buffer */
    const static unsigned long void_target_length = 2048;
    static uint8_t void_target_buffer[2048];
    if(void_target_length < length)
        return false;

    //TODO: Cache find_cipher
    err = eax_decrypt_verify_memory(find_cipher("rijndael"),
                                    (uint8_t *) key.data(), /* the key */
                                    (size_t)    key.size(), /* key is 16 bytes */
                                    (uint8_t *) nonce.data(), /* the nonce */
                                    (size_t)    nonce.size(), /* nonce is 16 bytes */
                                    (uint8_t *) header.data_ptr(), /* example header */
                                    (unsigned long) header.length(), /* header length */
                                    (const unsigned char *) data.data_ptr(),
                                    (unsigned long) data.length(),
                                    (unsigned char *) void_target_buffer,
                                    (unsigned char *) mac.data_ptr(),
                                    (unsigned long) mac.length(),
                                    &success
    );

    return err == CRYPT_OK && success;
}

#define tmp_buffer_size (2048)
bool CryptHandler::decrypt(const void *header, size_t header_length, void *payload, size_t payload_length, const void *mac, const key_t &key, const nonce_t &nonce, std::string &error) {
    if(tmp_buffer_size < payload_length) {
        error = "buffer too large";
        return false;
    }

    uint8_t tmp_buffer[tmp_buffer_size];
    int success;

    //TODO: Cache cipher
    auto err = eax_decrypt_verify_memory(find_cipher("rijndael"),
                                    (const uint8_t *) key.data(), /* the key */
                                    (unsigned long)    key.size(), /* key is 16 bytes */
                                    (const uint8_t *) nonce.data(), /* the nonce */
                                    (unsigned long)    nonce.size(), /* nonce is 16 bytes */
                                    (const uint8_t *) header, /* example header */
                                    (unsigned long) header_length, /* header length */
                                    (const unsigned char *) payload,
                                    (unsigned long) payload_length,
                                    (unsigned char *) tmp_buffer,
                                    (unsigned char *) mac,
                                    (unsigned long) 8,
                                    &success
    );
    if(err != CRYPT_OK) {
        error = "decrypt returned " + std::string{error_to_string(err)};
        return false;
    }

    if(!success) {
        error = "failed to verify packet";
        return false;
    }

    memcpy(payload, tmp_buffer, payload_length);
    return true;
}

bool CryptHandler::encrypt(
        const void *header, size_t header_length,
        void *payload, size_t payload_length,
        void *mac,
        const key_t &key, const nonce_t &nonce, std::string &error) {
    if(tmp_buffer_size < payload_length) {
        error = "buffer too large";
        return false;
    }

    uint8_t tmp_buffer[tmp_buffer_size];
    size_t tag_length{8};
    uint8_t tag_buffer[16];

    static_assert(sizeof(unsigned long) <= sizeof(tag_length));
    auto err = eax_encrypt_authenticate_memory(find_cipher("rijndael"),
                                          (uint8_t *) key.data(), /* the key */
                                          (unsigned long)    key.size(), /* key is 16 bytes */
                                          (uint8_t *) nonce.data(), /* the nonce */
                                          (unsigned long)    nonce.size(), /* nonce is 16 bytes */
                                          (uint8_t *) header, /* example header */
                                          (unsigned long) header_length, /* header length */
                                          (uint8_t *) payload, /* The plain text */
                                          (unsigned long) payload_length, /* Plain text length */
                                          (uint8_t *) tmp_buffer, /* The result buffer */
                                          (uint8_t *) tag_buffer,
                                          (unsigned long *)  &tag_length
    );
    assert(tag_length <= 16);

    if(err != CRYPT_OK) {
        error = "encrypt returned " + std::string{error_to_string(err)};
        return false;
    }

    memcpy(mac, tag_buffer, 8);
    memcpy(payload, tmp_buffer, payload_length);
    return true;
}
