#!/usr/bin/env bash

cd $(dirname $0)

# shellcheck disable=SC2034
glib_version="2.66.2"
install_prefix="$(pwd)/build_libraries/"

./build_glib.sh

# shellcheck disable=SC2181
[ $? -ne 0 ] && { exit 1; }

# Apply Cargo.toml fix for rust-webrtc dependencies
FIX_SCRIPT="$(pwd)/../../rtc/fix_cargo_deps.sh"
if [ -f "$FIX_SCRIPT" ]; then
    echo "Applying Cargo.toml fixes for rust-webrtc..."
    bash "$FIX_SCRIPT" || echo "Warning: Cargo fix script failed, continuing..."
fi

cargo update || exit 1

# Determine OpenSSL pkgconfig directory
openssl_pkgdir=$(pkg-config --variable=pcfiledir openssl)
if [ -z "$openssl_pkgdir" ]; then
    openssl_pkgdir="/usr/lib/x86_64-linux-gnu/pkgconfig"
fi

# rm -r target/release/
rbuild_install_prefix="$install_prefix" \
rbuild_library_type=static \
rbuild_libnice_gupnp=disabled \
PATH="$PATH:$install_prefix/bin" \
PKG_CONFIG_PATH="$install_prefix/lib/$(gcc -dumpmachine)/pkgconfig/:${openssl_pkgdir}/" \
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

# Determine OpenSSL library directory
openssl_libdir=$(pkg-config --variable=libdir openssl)
if [ -z "$openssl_libdir" ]; then
    openssl_libdir="/usr/lib/x86_64-linux-gnu"
fi

# shellcheck disable=SC2086
gcc -shared -o libteaspeak_rtc.so -Wl,--whole-archive target/release/libteaspeak_rtc.a -Wl,--no-whole-archive \
    $libraries \
     ${openssl_libdir}/libssl.so ${openssl_libdir}/libcrypto.so \
    -pthread -lm -lrt -lz -ldl -lresolv -static-libgcc \
    -Wl,--no-undefined,--gc-sections,--version-script=libteaspeakrtc.version

if [ $? -ne 0 ]; then
    echo "Failed to build shared library"
    exit 1
fi

#strip --strip-all librtc.so
