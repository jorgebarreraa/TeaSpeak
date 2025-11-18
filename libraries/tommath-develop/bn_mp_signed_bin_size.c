#include "tommath_private.h"
#ifdef BN_MP_SIGNED_BIN_SIZE_C
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

/* get the size for an signed equivalent */
int mp_signed_bin_size(const mp_int *a)
{
   return 1 + mp_unsigned_bin_size(a);
}
#endif

/* ref:         HEAD -> develop */
/* git commit:  a09c53619e0785adf17a597c9e7fd60bbb6ecb09 */
/* commit time: 2019-07-04 17:59:58 +0200 */
