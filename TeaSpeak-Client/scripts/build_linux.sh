#!/usr/bin/env bash

BASEDIR=$(dirname "$0")
cd "$BASEDIR/../"

if [[ "${teaclient_deploy_secret}" == "" ]]; then
    echo "Missing deploy secret!"
    exit 1
fi

npm run compile-tsc
if [[ $? -ne 0 ]]; then
    echo "Failed to compile project"
fi

npm run compile-sass
if [[ $? -ne 0 ]]; then
    echo "Failed to compile sass files"
fi

npm run build-linux-64
if [[ $? -ne 0 ]]; then
    echo "Failed to build project"
fi

npm run package-linux-64 $1
if [[ $? -ne 0 ]]; then
    echo "Failed to package project"
fi