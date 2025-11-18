#!/usr/bin/env bash

if [ -e "build.data" ]; then
    echo "File exists"
    DATA=$(cat "build.data")
else
    echo "Create new file"
    echo "0" > "build.data"
fi

DATA=$(($DATA+1))

echo "Data: $DATA"