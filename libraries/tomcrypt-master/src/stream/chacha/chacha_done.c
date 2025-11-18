/* LibTomCrypt, modular cryptographic library -- Tom St Denis
 *
 * LibTomCrypt is a library that provides various cryptographic
 * algorithms in a highly modular and flexible manner.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 */

#include "tomcrypt.h"

#ifdef LTC_CHACHA

/**
  Terminate and clear ChaCha state
  @param st      The ChaCha state
  @return CRYPT_OK on success
*/
int chacha_done(chacha_state *st)
{
   LTC_ARGCHK(st != NULL);
   XMEMSET(st, 0, sizeof(chacha_state));
   return CRYPT_OK;
}

#endif

/* ref:         HEAD -> master */
/* git commit:  0ff2920957a1687dd3804275fd3f29f41bfd7dd1 */
/* commit time: 2019-07-06 22:51:31 +0200 */
