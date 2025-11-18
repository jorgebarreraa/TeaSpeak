#!/usr/bin/env bash

BVERSION="$(cat env/buildVersion.txt)"
FNAME="TeaSpeak-$BVERSION.tar.gz"

if [ -d test ]; then
    echo "Deleting old test directory"
    rm -r test
fi

echo "Unpackaging release ($FNAME)"
mkdir test
tar xf $FNAME -C test/

cd test
#chmod +x teastart_minimal.sh
#./teastart_minimal.sh
LD_LIBRARY_PATH="./libs/"
./TeaSpeakServer
cd ..
#rm -r test
