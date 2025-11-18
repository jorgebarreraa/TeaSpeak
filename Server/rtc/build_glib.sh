#!/usr/bin/env bash

if [ -z "$glib_version" ]; then
    glib_version="2.66.2"
fi

if [ ! -d "glib-$glib_version" ]; then
    wget https://gitlab.gnome.org/GNOME/glib/-/archive/$glib_version/glib-$glib_version.tar.gz && \
    tar xvf glib-$glib_version.tar.gz && \
    rm glib-$glib_version.tar.gz

    if [ $? -ne 0 ]; then
        echo "Failed to download and extract glib"
        exit 1
    fi
fi
cd glib-$glib_version || exit

if [ ! -d "build_" ]; then
    # Enforce a zlib build
    # sed -i "s/libz_dep = dependency('zlib', required : false)/libz_dep = subproject('zlib').get_variable('zlib_dep')/g" meson.build

    # We're setting the PKG_CONFIG_LIBDIR to nowhere so glib does not finds any preinstalled libraries.
    sed -i "s/subdir('tests')/# subdir('tests')/g" gio/meson.build && \
    sed -i "s/subdir('fuzzing')/# subdir('fuzzing')/g" meson.build && \
    PKG_CONFIG_LIBDIR="$(pwd)" meson setup build_ \
        -Ddefault_library=static \
        -Dselinux=disabled \
        -Dlibmount=disabled \
        -Dinternal_pcre=true \
        -Dnls=disabled \
        --optimization=3 \
        --prefix="$(pwd)/../build_libraries/"

    if [ $? -ne 0 ]; then
        echo "Failed to setup meson build"
        exit 1
    fi
fi

meson install -C build_

if [ $? -ne 0 ]; then
    echo "Failed to build glib"
    exit 1
fi

cd ..