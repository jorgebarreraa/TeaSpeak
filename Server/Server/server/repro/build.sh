#!/bin/bash

BUILD_PATH=$1
if [[ -z "${BUILD_PATH}" ]]; then
    echo "Missing versions path!"
    exit 1
fi

rm buildVersion.txt
cp env/buildVersion.txt .
rm -r env
mkdir env
cd env

[[ $? -ne 0 ]] && {
  echo "Failed to create the env"
  exit 1
}
cp -rf ../../../git-teaspeak/default_files/{certs,commanddocs,geoloc,resources,*.sh} .
[[ $? -ne 0 ]] && {
  echo "Failed to copy env"
  exit 1
}

cp -rf ../../../music/bin/providers .
[[ $? -ne 0 ]] && {
  echo "Failed to copy providers"
  exit 1
}
#
cp ../../environment/TeaSpeakServer .
[[ $? -ne 0 ]] && {
  echo "Failed to copy server"
  exit 1
}
cd ..
mv buildVersion.txt env/buildVersion.txt
[[ $? -ne 0 ]] && {
  echo "Failed to move the build version back"
  exit 1
}

./generate_version.sh "${BUILD_PATH}" || {
    echo "Failed to generate version! ($?)"
    exit 1
}

./generate_libraries.sh || {
    echo "Failed to generate libraries! ($?)"
    exit 1
}


./make_symbol.sh || {
    echo "Failed to generate debug symbols"
    exit 1
}

./package_server.sh "${BUILD_PATH}" || {
    echo "Failed to package server! ($?)"
    exit 1
}

./deploy_build.sh "${BUILD_PATH}" || {
    echo "Failed to deploy package! ($?)"
    exit 1
}
