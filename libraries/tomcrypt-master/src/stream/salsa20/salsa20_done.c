/* LibTomCrypt, modular cryptographic library -- Tom St Denis
 *
 * LibTomCrypt is a library that provides various cryptographic
 * algorithms in a highly modular and flexible manner.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 */

#include "tomcrypt.h"

#ifdef LTC_SALSA20

/**
  Terminate and clear Salsa20 state
  @param st      The Salsa20 state
  @return CRYPT_OK on success
*/
int salsa20_done(salsa20_state *st)
{
   LTC_ARGCHK(st != NULL);
   XMEMSET(st, 0, sizeof(salsa20_state));
   return CRYPT_OK;
}

#endif

/* ref:         HEAD -> master */
/* git commit:  0ff2920957a1687dd3804275fd3f29f41bfd7dd1 */
/* commit time: 2019-07-06 22:51:31 +0200 */
