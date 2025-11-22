// Fast data compression library
// Copyright (C) 2006-2011 Lasse Mikkel Reinhold
// lar@quicklz.com
//
// QuickLZ can be used for free under the GPL 1, 2 or 3 license (where anything
// released into public must be open source) or under a commercial license if such
// has been acquired (see http://www.quicklz.com/order.html). The commercial license
// does not cover derived or ported versions created by third parties under GPL.

// 1.5.0 final

#include "QuickLZ.h"

#if QLZ_VERSION_MAJOR != 1 || QLZ_VERSION_MINOR != 5 || QLZ_VERSION_REVISION != 0
#error quicklz.c and quicklz.h have different versions
#endif

#if (defined(__X86__) || defined(__i386__) || defined(i386) || defined(_M_IX86) || defined(__386__) || defined(__x86_64__) || defined(_M_X64))
#define X86X64
#endif

size_t qlz_size_header(const char *source)
{
    size_t n = 2*((((*source) & 2) == 2) ? 4 : 1) + 1;
    return n;
}