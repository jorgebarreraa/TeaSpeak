#!/usr/bin/env bash

cd $(dirname $0)

install_prefix="$(pwd)/build_libraries/"
rbuild_install_prefix="$install_prefix" \
rbuild_library_type=static \
rbuild_libnice_gupnp=disabled \
PATH="$PATH:$install_prefix/bin" \
PKG_CONFIG_PATH="$install_prefix/lib/$(gcc -dumpmachine)/pkgconfig/" \
cargo rustc

cp target/debug/libteaspeak_rtc.so libteaspeak_rtc_development.so
