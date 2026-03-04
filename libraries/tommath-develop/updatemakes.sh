#!/bin/bash

./helper.pl --update-makefiles || exit 1

makefiles=(makefile makefile.shared makefile_include.mk makefile.msvc makefile.unix makefile.mingw)
vcproj=(libtommath_VS2008.vcproj)

if [ $# -eq 1 ] && [ "$1" == "-c" ]; then
  git add ${makefiles[@]} ${vcproj[@]} && git commit -m 'Update makefiles'
fi

exit 0

# ref:         HEAD -> develop
# git commit:  a09c53619e0785adf17a597c9e7fd60bbb6ecb09
# commit time: 2019-07-04 17:59:58 +0200
