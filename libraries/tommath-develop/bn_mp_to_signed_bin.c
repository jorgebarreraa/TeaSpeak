#include "tommath_private.h"
#ifdef BN_MP_TO_SIGNED_BIN_C
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

/* store in signed [big endian] format */
int mp_to_signed_bin(const mp_int *a, unsigned char *b)
{
   int     res;

   if ((res = mp_to_unsigned_bin(a, b + 1)) != MP_OKAY) {
      return res;
   }
   b[0] = (a->sign == MP_ZPOS) ? (unsigned char)0 : (unsigned char)1;
   return MP_OKAY;
}
#endif

/* ref:         HEAD -> develop */
/* git commit:  a09c53619e0785adf17a597c9e7fd60bbb6ecb09 */
/* commit time: 2019-07-04 17:59:58 +0200 */
