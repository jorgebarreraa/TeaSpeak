#include "tommath_private.h"
#ifdef BN_MP_SQRMOD_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis
 *
 * LibTomMath is a library that provides multiple-precision
 * integer arithmetic as well as number theoretic functionality.
 *
 * The library was designed directly after the MPI library by
 * Michael Fromberger but has been written from scratch with
 * additional optimizations in place.
 *
 * SPDX-License-Identifier: Unlicense
 */

/* c = a * a (mod b) */
int mp_sqrmod(const mp_int *a, const mp_int *b, mp_int *c)
{
   int     res;
   mp_int  t;

   if ((res = mp_init(&t)) != MP_OKAY) {
      return res;
   }

   if ((res = mp_sqr(a, &t)) != MP_OKAY) {
      mp_clear(&t);
      return res;
   }
   res = mp_mod(&t, b, c);
   mp_clear(&t);
   return res;
}
#endif

/* ref:         HEAD -> develop */
/* git commit:  a09c53619e0785adf17a597c9e7fd60bbb6ecb09 */
/* commit time: 2019-07-04 17:59:58 +0200 */
