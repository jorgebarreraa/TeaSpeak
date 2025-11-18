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
    exit 1
fi

npm run compile-sass
if [[ $? -ne 0 ]]; then
    echo "Failed to compile sass files"
    exit 1
fi

npm run build-windows-64
if [[ $? -ne 0 ]]; then
    echo "Failed to build project"
    exit 1
fi

npm run package-windows-64 $1
if [[ $? -ne 0 ]]; then
    echo "Failed to package project"
    exit 1
fi