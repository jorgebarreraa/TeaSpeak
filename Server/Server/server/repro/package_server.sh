#!/usr/bin/env bash

# shellcheck disable=SC2207
BUILD_INFO=($(cat build_version.txt))
BUILD_FULL_NAME=${BUILD_INFO[0]}
BUILD_NAME=${BUILD_INFO[1]}
BUILD_VERSION=${BUILD_INFO[2]}
BUILD_FILENAME=${BUILD_INFO[3]}

echo "Creating TeaSpeak ${BUILD_NAME} Build index ${BUILD_VERSION}"

if [[ -d finalenv ]]; then
  rm -r finalenv
fi
cp -r env finalenv
cd finalenv || {
  echo "failed to enter dir"
  exit 1
}
echo -e "# Version: ${BUILD_FULL_NAME}
# TeaSpeak version: ${BUILD_NAME}
# Build version: ${BUILD_VERSION}

{\"build_name\": \"${BUILD_FULL_NAME}\", \"build_version\": \"${BUILD_NAME}\", \"build_index\": ${BUILD_VERSION}}" > buildVersion.txt

echo "Stripping symbols"
strip -s -p -v TeaSpeakServer || { echo "failed to strip symbols!"; exit 1; }
patchelf --set-rpath ./libs/ TeaSpeakServer || { echo "failed to set rpath!"; exit 1; }

cd libs/ || exit 1
for file in *.so*; do
    echo "Editing rpath for $file"
    strip --strip-all "$file"
    patchelf --set-rpath "./libs/:./" "$file"
done
cd ..

tar --dereference -cvf - * | gzip -f -9 > "../${BUILD_FILENAME}"
[[ $? -ne 0 ]] && { echo "failed to package server"; exit 1; }

cd ..
rm -r finalenv

echo "Package created (${BUILD_FILENAME})"
