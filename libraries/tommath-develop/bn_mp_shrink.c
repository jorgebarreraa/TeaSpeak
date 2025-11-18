#include "tommath_private.h"
#ifdef BN_MP_SHRINK_C
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

/* shrink a bignum */
int mp_shrink(mp_int *a)
{
   mp_digit *tmp;
   int used = 1;

   if (a->used > 0) {
      used = a->used;
   }

   if (a->alloc != used) {
      if ((tmp = OPT_CAST(mp_digit) XREALLOC(a->dp, sizeof(mp_digit) * (size_t)used)) == NULL) {
         return MP_MEM;
      }
      a->dp    = tmp;
      a->alloc = used;
   }
   return MP_OKAY;
}
#endif

/* ref:         HEAD -> develop */
/* git commit:  a09c53619e0785adf17a597c9e7fd60bbb6ecb09 */
/* commit time: 2019-07-04 17:59:58 +0200 */
