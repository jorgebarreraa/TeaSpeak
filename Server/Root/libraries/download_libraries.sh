#!/bin/bash
set -e

echo "Downloading all TeaSpeak libraries..."
cd "$(dirname "$0")"

# Function to clone or skip if exists
clone_lib() {
    local url="$1"
    local dir="$2"
    local branch="$3"

    if [ -d "$dir" ]; then
        echo "  ✓ $dir already exists, skipping"
        return 0
    fi

    echo "  Cloning $dir..."
    if [ -n "$branch" ]; then
        git clone "$url" "$dir" --branch "$branch" --depth 1
    else
        git clone "$url" "$dir" --depth 1
    fi
}

clone_lib "https://github.com/open-source-parsers/jsoncpp.git" "jsoncpp"
clone_lib "https://git.did.science/WolverinDEV/ThreadPool.git" "Thread-Pool"
clone_lib "https://git.did.science/TeaSpeak/libraries/tomcrypt.git" "tomcrypt"
clone_lib "https://git.did.science/TeaSpeak/libraries/tommath.git" "tommath"
clone_lib "https://github.com/WolverinDEV/CXXTerminal.git" "CXXTerminal"
clone_lib "https://github.com/xiph/opus" "opus"
clone_lib "https://github.com/xiph/opusfile.git" "opusfile"
clone_lib "https://github.com/jbeder/yaml-cpp.git" "yaml-cpp"
clone_lib "https://github.com/libevent/libevent.git" "event"
clone_lib "https://git.did.science/TeaSpeak/libraries/spdlog.git" "spdlog"
clone_lib "https://github.com/WolverinDEV/StringVariable.git" "StringVariable"
clone_lib "https://github.com/WolverinDEV/ed25519.git" "ed25519"
clone_lib "https://chromium.googlesource.com/breakpad/breakpad" "breakpad"
clone_lib "https://boringssl.googlesource.com/boringssl" "boringssl"
clone_lib "https://fuchsia.googlesource.com/third_party/protobuf" "protobuf" "v3.5.1.1"
clone_lib "https://github.com/WolverinDEV/DataPipes.git" "DataPipes"
clone_lib "https://github.com/jemalloc/jemalloc.git" "jemalloc" "dev"
clone_lib "https://git.did.science/TeaSpeak/libraries/libnice-prebuild.git" "libnice"
clone_lib "https://git.did.science/TeaSpeak/libraries/glib2.0.git" "glibc"
clone_lib "https://git.did.science/TeaSpeak/libraries/openssl-prebuild.git" "openssl-prebuild"
clone_lib "https://github.com/facebook/zstd.git" "zstd"

# Download build-helpers
cd ..
clone_lib "https://github.com/WolverinDEV/build-helpers.git" "build-helpers"

echo "✓ All libraries downloaded successfully!"
