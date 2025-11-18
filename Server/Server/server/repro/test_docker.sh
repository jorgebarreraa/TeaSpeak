#!/usr/bin/env bash

BVERSION="$(cat env/buildVersion.txt)"
FBUILD=0

# shellcheck disable=SC2120
function buildName() {
    if [ -n "$1" ]; then
        L_FBUILD="$1"
    else
        L_FBUILD="${FBUILD}"
    fi
    if [ "$L_FBUILD" -eq "0" ]; then
       FULLNAME="TeaSpeak-$BVERSION"
    else
       FULLNAME="TeaSpeak-$BVERSION-$L_FBUILD"
    fi
}
FULLNAME=""
buildName

while [ -f "${FULLNAME}.tar.gz" ]
do
  FBUILD=$(($FBUILD+1))
  buildName
done
#Last known
FBUILD=$(($FBUILD-1))
buildName

echo "Got last release (${FULLNAME})"
echo "Copy to docker!"
docker cp ${FULLNAME}.tar.gz TeaSpeak-Test:/test/TeaSpeak.tar.gz
docker start -i TeaSpeak-Test