#include "tommath_private.h"
#ifdef BN_MP_DR_IS_MODULUS_C
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

/* determines if a number is a valid DR modulus */
int mp_dr_is_modulus(const mp_int *a)
{
   int ix;

   /* must be at least two digits */
   if (a->used < 2) {
      return 0;
   }

   /* must be of the form b**k - a [a <= b] so all
    * but the first digit must be equal to -1 (mod b).
    */
   for (ix = 1; ix < a->used; ix++) {
      if (a->dp[ix] != MP_MASK) {
         return 0;
      }
   }
   return 1;
}

#endif

/* ref:         HEAD -> develop */
/* git commit:  a09c53619e0785adf17a597c9e7fd60bbb6ecb09 */
/* commit time: 2019-07-04 17:59:58 +0200 */
