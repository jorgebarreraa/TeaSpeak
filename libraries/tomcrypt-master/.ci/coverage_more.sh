#!/bin/bash

set -e

if [ "$#" = "1" -a "$(echo $1 | grep 'gmp')" != "" ]; then
   ./test t gmp
fi

./sizes
./constants

for i in $(for j in $(echo $(./hashsum -h | awk '/Algorithms/,EOF' | tail -n +2)); do echo $j; done | sort); do echo -n "$i: " && ./hashsum -a $i tests/test.key ; done > hashsum_tv.txt
difftroubles=$(diff -i -w -B hashsum_tv.txt notes/hashsum_tv.txt | grep '^<') || true
if [ -n "$difftroubles" ]; then
  echo "FAILURE: hashsum_tv.tx"
  diff -i -w -B hashsum_tv.txt notes/hashsum_tv.txt
  echo "hashsum failed"
  exit 1
else
  echo "hashsum okay"
fi


exit 0

# ref:         HEAD -> master
# git commit:  0ff2920957a1687dd3804275fd3f29f41bfd7dd1
# commit time: 2019-07-06 22:51:31 +0200
