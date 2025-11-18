#include <openssl/sha.h>
#include <stdlib.h>
#include "../include/sha512.h"


int _ed_sha512_init(sha512_context* md) {
	md->context = malloc(sizeof(SHA512_CTX));
	return SHA512_Init(md->context) != 1; /* Returns 0 on success */
}

int _ed_sha512_final(sha512_context* md, unsigned char *out) {
	assert(md->context);

	int result = SHA512_Final(out, md->context) != 1; /* Returns 0 on success */
	free(md->context);
	md->context = 0;
	return result;
}

int _ed_sha512_update(sha512_context* md, const unsigned char *in, size_t inlen) {
	assert(md->context);

	return SHA512_Update(md->context, in, inlen) != 1; /* Returns 0 on success */
}


#ifdef WIN32
__declspec(dllexport)
#endif
sha512_functions _ed_sha512_functions = {
		_ed_sha512_init,
		_ed_sha512_final,
		_ed_sha512_update
};