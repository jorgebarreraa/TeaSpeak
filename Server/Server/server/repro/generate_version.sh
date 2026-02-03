#!/usr/bin/env bash

BUILD_PATH=$1

function debug() {
    #eval ""
    echo "${@}"
}

VERSION_FILE="build_version.txt"
if [[ -f ${VERSION_FILE} ]]; then
    rm ${VERSION_FILE}
fi

if [[ -z ${BUILD_PATH} ]]; then
    echo "Missing versions path!"
    #exit 1
fi

CURRENT_VERSION=$(cat env/buildVersion.txt)
CURRENT_VERSION_ESCAPED=$(echo "${CURRENT_VERSION}" | sed -e 's/[\/&\.\-]/\\&/g')
AVAILABLE_VERSIONS=$(ssh -i build_private_key TeaSpeak-Jenkins@mcgalaxy.de "
if [ -d versions/${BUILD_PATH} ]; then
    ls versions/${BUILD_PATH} | grep -E '^${CURRENT_VERSION_ESCAPED}(\-[0-9]+)?$'
fi
")
debug "${AVAILABLE_VERSIONS}"

TARGET_VERSION=""
TARGET_VERSION_INDEX=0

while [[ true ]]; do
    if [[ ! ${TARGET_VERSION_INDEX} -eq 0 ]]; then
        TARGET_VERSION="${CURRENT_VERSION}-${TARGET_VERSION_INDEX}"
    else
        TARGET_VERSION="${CURRENT_VERSION}"
    fi
    debug "Testing => ${TARGET_VERSION}"
    debug "${AVAILABLE_VERSIONS}" | grep "${TARGET_VERSION}" &>/dev/null
    if [[ $? -ne 0 ]]; then
        debug "Found version ${TARGET_VERSION}"
        break
    fi

    TARGET_VERSION_INDEX=$(($TARGET_VERSION_INDEX+1))
done

echo "${TARGET_VERSION} ${CURRENT_VERSION} ${TARGET_VERSION_INDEX} TeaSpeak-${TARGET_VERSION}.tar.gz" > ${VERSION_FILE}