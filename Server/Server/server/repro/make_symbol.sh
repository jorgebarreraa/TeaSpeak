#!/bin/bash

SYMBOL_ROOT="symbols"
BINARY_PATH="env"
TMP_FILE="temp"

function mkdir_not_exists() {
    if [[ ! -d $1 ]]; then
        mkdir "$1"
    fi
}

function create_dump() {
    local BINARY_PATH=${1}
    local BINARY_NAME=${2}

    echo "Creating dump file for ${BINARY_NAME} (${BINARY_PATH}/${BINARY_NAME})"
    dump_syms "${BINARY_PATH}/${BINARY_NAME}" > ${TMP_FILE} || {
        echo "Failed to generate dump."
        exit 1
    }
    SYM_INFO=$(head -n1 < ${TMP_FILE})
    SYM_INFO=($SYM_INFO)
    DUMP_ID=${SYM_INFO[3]}

    echo "Dump ID: $DUMP_ID"
    mkdir_not_exists ${SYMBOL_ROOT}
    mkdir_not_exists ${SYMBOL_ROOT}/${BINARY_NAME}
    mkdir_not_exists ${SYMBOL_ROOT}/${BINARY_NAME}/${DUMP_ID}

    DUMP_PATH=${SYMBOL_ROOT}/${BINARY_NAME}/${DUMP_ID}/${BINARY_NAME}.sym
    mv "$TMP_FILE" "$DUMP_PATH" || {
        echo "Failed to move dump."
        exit 1
    }
}

create_dump "env" "TeaSpeakServer"
create_dump "env/providers" "000ProviderFFMpeg.so"
create_dump "env/providers" "001ProviderYT.so"
create_dump "env/libs/" "libteaspeak_rtc.so"
echo "Created dump symbols!"
