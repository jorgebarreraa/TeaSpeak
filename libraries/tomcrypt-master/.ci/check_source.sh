#!/bin/bash

# output version
bash .ci/printinfo.sh

make clean > /dev/null

echo "checking..."
./helper.pl --check-source --check-makefiles --check-defines|| exit 1

exit 0

# ref:         HEAD -> master
# git commit:  0ff2920957a1687dd3804275fd3f29f41bfd7dd1
# commit time: 2019-07-06 22:51:31 +0200
