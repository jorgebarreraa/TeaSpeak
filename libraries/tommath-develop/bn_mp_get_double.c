#include "tommath_private.h"
#ifdef BN_MP_GET_DOUBLE_C
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

double mp_get_double(const mp_int *a)
{
   int i;
   double d = 0.0, fac = 1.0;
   for (i = 0; i < DIGIT_BIT; ++i) {
      fac *= 2.0;
   }
   for (i = USED(a); i --> 0;) {
      d = (d * fac) + (double)DIGIT(a, i);
   }
   return (mp_isneg(a) != MP_NO) ? -d : d;
}
#endif

/* ref:         HEAD -> develop */
/* git commit:  a09c53619e0785adf17a597c9e7fd60bbb6ecb09 */
/* commit time: 2019-07-04 17:59:58 +0200 */
