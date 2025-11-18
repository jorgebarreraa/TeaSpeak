#include "tommath_private.h"
#ifdef BN_MP_CLEAR_MULTI_C
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

#include <stdarg.h>

void mp_clear_multi(mp_int *mp, ...)
{
   mp_int *next_mp = mp;
   va_list args;
   va_start(args, mp);
   while (next_mp != NULL) {
      mp_clear(next_mp);
      next_mp = va_arg(args, mp_int *);
   }
   va_end(args);
}
#endif

/* ref:         HEAD -> develop */
/* git commit:  a09c53619e0785adf17a597c9e7fd60bbb6ecb09 */
/* commit time: 2019-07-04 17:59:58 +0200 */
