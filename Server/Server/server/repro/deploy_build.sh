#!/usr/bin/env bash

BUILD_PATH=$1
if [[ -z ${BUILD_PATH} ]]; then
    echo "Missing versions path!"
    exit 1
fi

BUILD_INFO=($(cat build_version.txt))
BUILD_FULL_NAME=${BUILD_INFO[0]}
BUILD_NAME=${BUILD_INFO[1]}
BUILD_VERSION=${BUILD_INFO[2]}
BUILD_FILENAME=${BUILD_INFO[3]}

echo "Publishing build ${BUILD_FILENAME}"
if [[ ! -f ${BUILD_FILENAME} ]]; then
    echo "Failed to find file!"
    exit 1
fi

if [[ -d symbols ]]; then
    echo "Uploading symbols"
    scp -i build_private_key -rpC symbols/ TeaSpeak-Jenkins@mcgalaxy.de:symbols/
    if [[ $? -ne 0 ]]; then
        echo "Failed to upload symbols!"
        exit 1
    fi
    rm -r symbols/
else
    echo "Failed to find symbols! Skipping step!"
fi

echo "Creating versions mark"
ssh -i build_private_key TeaSpeak-Jenkins@mcgalaxy.de "
if [ ! -d versions/${BUILD_PATH} ]; then
    mkdir -p versions/${BUILD_PATH}
fi
if [ ! -d files/${BUILD_PATH} ]; then #Creating for files as well
    mkdir -p files/${BUILD_PATH}
fi
echo '' > versions/${BUILD_PATH}/${BUILD_FULL_NAME}
echo '${BUILD_FULL_NAME}' > versions/${BUILD_PATH}/latest"
if [[ $? -ne 0 ]]; then
    echo "Failed to create versions mark!"
    exit 1
fi

echo "Uploading build (${BUILD_FILENAME})"
scp -i build_private_key -pC "${BUILD_FILENAME}" "TeaSpeak-Jenkins@mcgalaxy.de:files/${BUILD_PATH}/"
if [[ $? -ne 0 ]]; then
    echo "Failed to upload version!"
    exit 1
fi
