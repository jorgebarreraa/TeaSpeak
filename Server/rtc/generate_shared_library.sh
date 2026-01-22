#!/usr/bin/env bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# shellcheck disable=SC2034
glib_version="2.66.2"
install_prefix="$(pwd)/build_libraries/"

./build_glib.sh

# shellcheck disable=SC2181
[ $? -ne 0 ] && { exit 1; }

# Apply Cargo.toml fix for rust-webrtc dependencies
FIX_SCRIPT="$SCRIPT_DIR/fix_cargo_deps.sh"
if [ -f "$FIX_SCRIPT" ]; then
    echo "Applying Cargo.toml fixes for rust-webrtc..."
    bash "$FIX_SCRIPT" || echo "Warning: Cargo fix script failed, continuing..."
else
    echo "Warning: Cargo fix script not found at $FIX_SCRIPT"
fi

# Determine OpenSSL pkgconfig directory FIRST
openssl_pkgdir=$(pkg-config --variable=pcfiledir openssl)
if [ -z "$openssl_pkgdir" ]; then
    openssl_pkgdir="/usr/lib/x86_64-linux-gnu/pkgconfig"
fi

# Determine OpenSSL library directory
openssl_libdir=$(pkg-config --variable=libdir openssl)
if [ -z "$openssl_libdir" ]; then
    openssl_libdir="/usr/lib/x86_64-linux-gnu"
fi

# Set environment variables BEFORE any cargo commands
export OPENSSL_NO_VENDOR=1
export OPENSSL_LIB_DIR="${openssl_libdir}"
export OPENSSL_INCLUDE_DIR="/usr/include"
export PKG_CONFIG_PATH="$install_prefix/lib/$(gcc -dumpmachine)/pkgconfig/:${openssl_pkgdir}/"
export PATH="$PATH:$install_prefix/bin"

# Clean ALL cargo artifacts and caches - including incremental compilation
echo "Cleaning ALL cargo caches and build artifacts..."
rm -rf target/
rm -rf ~/.cargo/registry/cache/*
rm -rf ~/.cargo/registry/src/*

# Also remove any cached build scripts
find ~/.cargo/registry -name "build-script-build" -delete 2>/dev/null || true

# First update and fetch all dependencies
cargo update || exit 1
echo "Fetching all Cargo dependencies (including git repos)..."
cargo fetch || exit 1

# Apply Cargo.toml fix for rust-webrtc dependencies AFTER cargo clones the repo
FIX_SCRIPT="$SCRIPT_DIR/fix_cargo_deps.sh"
if [ -f "$FIX_SCRIPT" ]; then
    echo "Applying Cargo.toml fixes and Rust API patches..."
    bash "$FIX_SCRIPT" || echo "Warning: Cargo fix script failed, continuing..."
else
    echo "Warning: Cargo fix script not found at $FIX_SCRIPT"
fi

# Try to build - this will create all checkouts if they don't exist
echo "Attempting first build (may fail if checkouts need patches)..."
rbuild_install_prefix="$install_prefix" \
rbuild_library_type=static \
rbuild_libnice_gupnp=disabled \
cargo build --release

# If first build failed, apply patches again and retry
if [ $? -ne 0 ]; then
    echo ""
    echo "════════════════════════════════════════════════════════════"
    echo "  First build failed - applying patches to new checkouts"
    echo "════════════════════════════════════════════════════════════"

    # Apply patches to newly created checkouts
    if [ -f "$FIX_SCRIPT" ]; then
        bash "$FIX_SCRIPT" || echo "Warning: Cargo fix script failed"
    fi

    echo ""
    echo "Retrying build with patched dependencies..."
    rbuild_install_prefix="$install_prefix" \
    rbuild_library_type=static \
    rbuild_libnice_gupnp=disabled \
    cargo build --release

    if [ $? -ne 0 ]; then
        echo "Failed to build after applying patches"
        exit 1
    fi
fi

if [ ! -e "target/release/libteaspeak_rtc.a" ]; then
    echo "Missing libteaspeak_rtc.a"
fi

library_path="$install_prefix/lib/$(gcc -dumpmachine)/"
if [ ! -d "$library_path" ]; then
    echo "Missing host triplet directory ($library_path)"
    exit 1
fi

static_libraries=(
    "libusrsctp.a"
    "libsrtp2.a"
    "libnice.a"
    "libgio-2.0.a"
    "libgobject-2.0.a"
    "libgmodule-2.0.a"
    "libglib-2.0.a"
    "libffi.a"
)

libraries=""
for library in "${static_libraries[@]}"
do
    if [ ! -e "$library_path/$library" ]; then
        echo "Missing static library ${library} ($library_path/$library)"
        exit 1
    fi

    libraries="$libraries $library_path/$library"
done

# shellcheck disable=SC2086
gcc -shared -o libteaspeak_rtc.so -Wl,--whole-archive target/release/libteaspeak_rtc.a -Wl,--no-whole-archive \
    $libraries \
    -L${openssl_libdir} -lssl -lcrypto \
    -pthread -lm -lrt -lz -ldl -lresolv -static-libgcc \
    -Wl,--no-undefined,--gc-sections,--version-script=libteaspeakrtc.version

if [ $? -ne 0 ]; then
    echo "Failed to build shared library"
    exit 1
fi

#strip --strip-all librtc.so
