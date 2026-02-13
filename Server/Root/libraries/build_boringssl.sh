#!/bin/bash
# Standalone BoringSSL build script for TeaSpeak.
# Run this script from Server/Root/libraries/ to build BoringSSL.
# Output: boringssl/lib/libssl.a and boringssl/lib/libcrypto.a
#
# Usage:
#   cd Server/Root/libraries
#   bash build_boringssl.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

BORINGSSL_DIR="$SCRIPT_DIR/boringssl"

if [[ ! -d "$BORINGSSL_DIR" ]]; then
    echo "ERROR: BoringSSL source not found at $BORINGSSL_DIR"
    echo "Run download_libraries_custom.sh first to clone BoringSSL."
    exit 1
fi

# Check if already built
if [[ -f "$BORINGSSL_DIR/lib/libssl.a" && -f "$BORINGSSL_DIR/lib/libcrypto.a" ]]; then
    echo "BoringSSL already built at $BORINGSSL_DIR/lib/"
    exit 0
fi

echo "Building BoringSSL..."

# Install Go if not present (required for BoringSSL code generation)
if ! command -v go &>/dev/null; then
    echo "Go not found, attempting to install golang-go..."
    apt-get install -y golang-go 2>/dev/null || {
        echo "WARNING: Could not install golang-go. BoringSSL build may fail."
    }
fi

BUILD_DIR="$BORINGSSL_DIR/_build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake "$BORINGSSL_DIR" \
    -DOPENSSL_NO_ASM=ON \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_C_FLAGS="-fPIC" \
    -DCMAKE_CXX_FLAGS="-fPIC -Wno-error=format -Wno-error=format-extra-args" \
    -DCMAKE_VERBOSE_MAKEFILE=ON

make -j"$(nproc)" ssl crypto

# Copy outputs to expected location
mkdir -p "$BORINGSSL_DIR/lib"
cp ssl/libssl.a "$BORINGSSL_DIR/lib/libssl.a"
cp crypto/libcrypto.a "$BORINGSSL_DIR/lib/libcrypto.a"

echo "BoringSSL built successfully!"
echo "  $BORINGSSL_DIR/lib/libssl.a"
echo "  $BORINGSSL_DIR/lib/libcrypto.a"
