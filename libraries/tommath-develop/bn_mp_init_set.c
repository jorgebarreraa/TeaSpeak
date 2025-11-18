#include "tommath_private.h"
#ifdef BN_MP_INIT_SET_C
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

/* initialize and set a digit */
int mp_init_set(mp_int *a, mp_digit b)
{
   int err;
   if ((err = mp_init(a)) != MP_OKAY) {
      return err;
   }
   mp_set(a, b);
   return err;
}
#endif

/* ref:         HEAD -> develop */
/* git commit:  a09c53619e0785adf17a597c9e7fd60bbb6ecb09 */
/* commit time: 2019-07-04 17:59:58 +0200 */
