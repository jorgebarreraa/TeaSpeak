#include "tommath_private.h"
#ifdef BN_MP_COMPLEMENT_C
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

/* b = ~a */
int mp_complement(const mp_int *a, mp_int *b)
{
   int res = mp_neg(a, b);
   return (res == MP_OKAY) ? mp_sub_d(b, 1uL, b) : res;
}
#endif

/* ref:         HEAD -> develop */
/* git commit:  a09c53619e0785adf17a597c9e7fd60bbb6ecb09 */
/* commit time: 2019-07-04 17:59:58 +0200 */
