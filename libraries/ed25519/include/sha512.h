#ifndef SHA512_H
#define SHA512_H

#include <assert.h>

typedef struct {
	void* context;
} sha512_context;

typedef struct sha512_functions_ {
	int(*_ed_sha512_init)(sha512_context*);
	int(*_ed_sha512_final)(sha512_context*, unsigned char *);
	int(*_ed_sha512_update)(sha512_context*, const unsigned char *, size_t);
} sha512_functions;

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WIN32
__declspec(dllexport)
#endif
extern sha512_functions _ed_sha512_functions;

#ifdef __cplusplus
}
#endif


inline void _ed_sha512_validate() {
    assert(_ed_sha512_functions._ed_sha512_init);
    assert(_ed_sha512_functions._ed_sha512_final);
    assert(_ed_sha512_functions._ed_sha512_update);
}
inline int _ed_sha512(const unsigned char *message, size_t message_len, unsigned char *out) {
    _ed_sha512_validate();

    int result = 1;
    sha512_context ctx;
    result &= _ed_sha512_functions._ed_sha512_init(&ctx);
    result &= _ed_sha512_functions._ed_sha512_update(&ctx, message, message_len);
    result &= _ed_sha512_functions._ed_sha512_final(&ctx, out);
    return result;
}

#endif
