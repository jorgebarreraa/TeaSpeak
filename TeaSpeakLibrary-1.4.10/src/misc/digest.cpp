#ifndef NO_OPEN_SSL
#define NO_OPEN_SSL
#endif

#include "./digest.h"
#include <tomcrypt.h>

#define DECLARE_DIGEST(name, digestLength)                                                                  \
void digest::tomcrypt::name(const char* input, size_t length, uint8_t* result) {                            \
    hash_state hash{};                                                                                      \
                                                                                                            \
    name ##_init(&hash);                                                                                    \
    name ##_process(&hash, (uint8_t*) input, (unsigned long) length);                                       \
    name ##_done(&hash, result);                                                                            \
}

DECLARE_DIGEST(sha1, SHA_DIGEST_LENGTH)
DECLARE_DIGEST(sha256, SHA256_DIGEST_LENGTH)
DECLARE_DIGEST(sha512, SHA512_DIGEST_LENGTH)