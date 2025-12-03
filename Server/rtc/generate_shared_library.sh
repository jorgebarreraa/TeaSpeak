#!/usr/bin/env bash

cd $(dirname $0)

# shellcheck disable=SC2034
glib_version="2.66.2"
install_prefix="$(pwd)/build_libraries/"
./build_glib.sh

# shellcheck disable=SC2181
[ $? -ne 0 ] && { exit 1; }

cargo update || exit 1

# Verify OpenSSL environment variables are set
if [ -z "$OPENSSL_DIR" ]; then
    echo "ERROR: OPENSSL_DIR is not set. Please run this script from build_teaspeak.sh"
    exit 1
fi

echo "Using OpenSSL from: $OPENSSL_DIR"

# rm -r target/release/
rbuild_install_prefix="$install_prefix" \
rbuild_library_type=static \
rbuild_libnice_gupnp=disabled \
PATH="$PATH:$install_prefix/bin" \
PKG_CONFIG_PATH="$install_prefix/lib/$(gcc -dumpmachine)/pkgconfig/:$crypto_library_path/lib/pkgconfig/" \
OPENSSL_DIR="$OPENSSL_DIR" \
OPENSSL_LIB_DIR="$OPENSSL_LIB_DIR" \
OPENSSL_INCLUDE_DIR="$OPENSSL_INCLUDE_DIR" \
OPENSSL_STATIC="$OPENSSL_STATIC" \
OPENSSL_NO_PKG_CONFIG="$OPENSSL_NO_PKG_CONFIG" \
cargo rustc --release

if [ $? -ne 0 ]; then
    echo "Failed to build glib"
    exit 1
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
     $crypto_library_path/lib/libssl.so $crypto_library_path/lib/libcrypto.so \
    -pthread -lm -lrt -lz -ldl -lresolv -static-libgcc \
    -Wl,--no-undefined,--gc-sections,--version-script=libteaspeakrtc.version

if [ $? -ne 0 ]; then
    echo "Failed to build shared library"
    exit 1
fi

#strip --strip-all librtc.so
