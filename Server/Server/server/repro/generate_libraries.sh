#!/bin/bash

#Required libraries:
# "libssl.so"
# "libcrypto.so"
# "libjemalloc.so.2"
# "libsqlite3.so.0"
# "libTeaMusic.so"
# "libteaspeak_rtc.so"

[[ -z "${build_os_type}" ]] && { echo "missing build os type"; exit 1; }
[[ -z "${build_os_arch}" ]] && { echo "missing build os arch"; exit 1; }
build_path="out/${build_os_type}_${build_os_arch}/"

function query_system_link() {
    local _binary="$2"
    [[ -z "$_binary" ]] && _binary="../../../environment/TeaSpeakServer"
    data=$(ldd "$_binary" | grep "$1")
    # shellcheck disable=SC2206
    data=($data)
    library_path=${data[2]}

    [[ -z "${library_path}" ]] && { echo "failed to resolve library path for $1 in $_binary"; exit 1; }
    [[ "${library_path}" == "not" ]] && { echo "failed to resolve library path for $1 in $_binary"; exit 1; }
}

cd $(dirname $0) || exit 1
cd env || { echo "failed to enter env"; exit 1; }

[[ -d libs ]] && {
    rm -r libs || { echo "failed to delete old lib folder"; exit 1; }
}
mkdir libs || { echo "failed to create lib directory"; exit 1; }
cd libs || { echo "failed to enter lib directory"; exit 1; }

# Creating copied
library_base=$(realpath ../../../../../libraries)

# Setting up ssl
query_system_link "libssl.so.1.1"
cp "${library_path}" . || { echo "failed to copy libssl.so.1.1"; exit 1; }

# Setting up crypto
query_system_link "libcrypto.so.1.1"
cp "${library_path}" . || { echo "failed to copy libcrypto.so.1.1"; exit 1; }

# Setting up Sqlite3
query_system_link "libsqlite3.so.0"
cp "${library_path}" . || { echo "failed to copy libsqlite3.so.0"; exit 1; }

# Setting up jemalloc
query_system_link "libjemalloc.so.2"
cp "${library_path}" . || { echo "failed to copy libjemalloc.so.2"; exit 1; }

# Setting up TeaMusic
library_path=$(realpath "../../../../MusicBot/libs/libTeaMusic.so")
cp "$library_path" . || { echo "failed to copy libTeaMusic.so"; exit 1; }

query_system_link "libteaspeak_rtc.so"
cp "${library_path}" . || { echo "failed to copy libteaspeak_rtc.so"; exit 1; }

# Doing some prostprocessing
chmod 755 *
echo "All libraries have been copied successfully"
