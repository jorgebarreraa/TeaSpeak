#!/usr/bin/env bash

[[ -z "${tommath_library}" ]] && tommath_library="$(pwd)/../tommath/build/libtommathStatic.a"
[[ -z "${tommath_include}" ]] && tommath_include="../tommath/"

make -f makefile clean
make -f makefile CFLAGS="-fPIC -DUSE_LTM -DLTM_DESC -I${tommath_include}" EXTRALIBS="${tommath_library}" VERBOSE=1 ${CMAKE_MAKE_OPTIONS}
make PREFIX=./out/${build_os_type}_${build_os_arch}/ install
