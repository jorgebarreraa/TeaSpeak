#pragma once

#include <cassert>

//#define ALLOW_ASSERT
#ifdef ALLOW_ASSERT
    #define sassert(exp) assert(exp)
#else
    #define S(s) #s
    #define sassert(exp)                                                                                                \
    do {                                                                                                                \
        if(!(exp)) {                                                                                                    \
            logCritical(0,                                                                                              \
                        "Soft assertion @{}:{} '{}' failed! This could cause fatal fails!",                             \
                        __FILE__, __LINE__, #exp);                                                                      \
        }                                                                                                               \
    } while(0)
    #undef S
#endif