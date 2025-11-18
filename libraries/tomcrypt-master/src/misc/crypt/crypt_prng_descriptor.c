/* LibTomCrypt, modular cryptographic library -- Tom St Denis
 *
 * LibTomCrypt is a library that provides various cryptographic
 * algorithms in a highly modular and flexible manner.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 */
#include "tomcrypt.h"

/**
  @file crypt_prng_descriptor.c
  Stores the PRNG descriptors, Tom St Denis
*/
struct ltc_prng_descriptor prng_descriptor[TAB_SIZE] = {
{ NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
};

LTC_MUTEX_GLOBAL(ltc_prng_mutex)


/* ref:         HEAD -> master */
/* git commit:  0ff2920957a1687dd3804275fd3f29f41bfd7dd1 */
/* commit time: 2019-07-06 22:51:31 +0200 */
