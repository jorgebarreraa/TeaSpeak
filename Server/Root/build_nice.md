# Dependencies
## General dependencies
```shell script
sudo apt-get install -y texinfo bison flex libglib2.0-dev
apt-get install gtk-doc-tools # For libnice later
```

## Python 3.6
```shell script
wget https://www.python.org/ftp/python/3.6.3/Python-3.6.3.tgz && \
tar -xvf Python-3.6.3.tgz && \
cd Python-3.6.3 && \
sudo ./configure --enable-optimizations && \
make -j`nproc --all` && \
sudo make install && \
sudo update-alternatives --install /usr/bin/python python /usr/local/bin/python3.6 60 && \
sudo update-alternatives --install /usr/bin/python python /usr/bin/python2.7 50

# You may need to set the default python to 2.7 for this step (if "lsb_release -a" fails):
# sudo update-alternatives --config python
pip3.6 install --upgrade pip
```

## Ninja (from source)
```shell script
apt-get purge ninja

git clone git://github.com/ninja-build/ninja.git && \
cd ninja && \
git checkout release && \
./configure.py --bootstrap && \
mv ninja /usr/local/bin/
```

## Meson
```shell script
pip3.6 install meson
```

## objcopy
`objcopy` must be build from source because the option "--add-symbol" is mouse likely not available...  
You might have to append `--target=i386-pc-linux-gnu` for 32 bit PCs  
```shell script
git clone git://sourceware.org/git/binutils-gdb.git && cd binutils-gdb && \
CXXFLAGS="-Wno-error=unused-function -O2" CFLAGS="-Wno-error=unused-function -O2" ./configure && \
make -j`nproc --all` && \
make install && \
cd ..
```

# Building GLIB2.0
Building GLIB2.0
```shell script
git clone https://github.com/GNOME/glib.git && \
cd glib && \
git checkout glib-2-62 && \
meson _build -Dlibmount=false --prefix=`pwd`/out --buildtype=release --default-library=both -Diconv=libc -Dselinux=disabled

ninja -C _build && \
ninja -C _build install && \
cd ..
```

Publishing GLIB2.0
```shell script
export build_os_type=linux
export build_os_arch=amd64
export prebuild_path="`pwd`/glibc-prebuild/${build_os_type}_${build_os_arch}/"

git clone https://git.did.science/WolverinDEV/glibc-prebuild.git
[[ -d $prebuild_path ]] && rm -r "$prebuild_path"
mkdir $prebuild_path && \
cp -r glib/out/include/ $prebuild_path && \
cp -r glib/out/lib/ $prebuild_path
```

# Building libnice
Building boringssl (not needed anymore!)
```shell script
git clone https://boringssl.googlesource.com/boringssl && cd boringssl && \
git checkout 1cc95ac0 # This version does TeaSpeak use

[[ "`getconf LONG_BIT`" != "64" && ! "$(uname -m)" =~ arm* ]] && { export _toolchain="-DCMAKE_TOOLCHAIN_FILE=`pwd`/util/32-bit-toolchain.cmake"; }
[[ ! $(cat ssl/test/bssl_shim.cc | grep "__STDC_FORMAT_MACROS" &> /dev/null) ]] && echo "`echo -e "#define __STDC_FORMAT_MACROS\n\n"``cat ssl/test/bssl_shim.cc`" > ssl/test/bssl_shim.cc
cmake . -DCMAKE_INSTALL_PREFIX=`pwd`/out -DOPENSSL_NO_ASM=ON -DCMAKE_CXX_FLAGS="-Wno-error=attributes" -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release $_toolchain && \
make -j`nproc --all`
```

Building openssl
```shell script
[[ -z "${build_os_type}" ]] && export build_os_type=linux
[[ -z "${build_os_type}" ]] && export build_os_arch=amd64

git clone git://git.openssl.org/openssl.git && cd openssl && \
git checkout OpenSSL_1_1_1-stable && \
mkdir build && cd build && \
../Configure --prefix=`pwd`/../out linux-x86_64 && \
make -j32 && \
make install && \
cd ..

# Update prebuild biraries
[[ ! -d openssl-prebuild ]] && git clone https://git.did.science/WolverinDEV/openssl-prebuild.git
cd openssl-prebuild && \
if [[ ! -d "${build_os_type}_${build_os_arch}" ]]; then 
  mkdir "${build_os_type}_${build_os_arch}" || { echo "failed to create directory"; }
fi
cd "${build_os_type}_${build_os_arch}" && \
rm -r * && \
cp -r ../../openssl/out/* .
```

Build libnice
```shell script
[[ -z "${build_os_type}" ]] && export build_os_type=linux
[[ -z "${build_os_type}" ]] && export build_os_arch=amd64

[[ ! -d libnice]] && git clone https://github.com/libnice/libnice.git && cd libnice

export glib20_dir="`pwd`/../glibc-prebuild/${build_os_type}_${build_os_arch}/"
export glib20_lib_path=$(realpath "$glib20_dir/lib/"*"/")
export boringssl_path="`pwd`/../boringssl/"
export openssl_path="`pwd`/../openssl/out/"

#pkg-config --static --cflags glib-2.0 := -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include
#pkg-config --static --libs glib-2.0 gio-2.0 gobject-2.0 := -pthread -lgio-2.0 -lz -lresolv -lselinux -lgmodule-2.0 -ldl -lgobject-2.0 -lffi -lglib-2.0 -lpcre
export GLIB_CFLAGS="-I$glib20_dir/include/glib-2.0 -I`echo "$glib20_dir/lib/"*"/glib-2.0/include"`"
#export GLIB_LIBS="$glib20_lib_path/libgio-2.0.a -lz -lresolv $glib20_lib_path/libgmodule-2.0.a -ldl $glib20_lib_path/libgobject-2.0.a $glib20_lib_path/libffi.a $glib20_lib_path/libglib-2.0.a -lpcre -pthread" # Static
export GLIB_LIBS="-lgio-2.0 -lz -lresolv -lgmodule-2.0 -ldl -lgobject-2.0 -lffi -lglib-2.0 -lpcre -pthread" # Shared
# For BoringSSL: export LDFLAGS="-L$boringssl_path/crypto/ -L$boringssl_path/ssl/ -L$glib20_lib_path"
export LDFLAGS="-L$openssl_path/lib -L$glib20_lib_path"

# sed -i 's/gtkdocize/#gtkdocize/g' autogen.sh # Fix for ARM
sed -i 's/aclocal --install/aclocal --install -I`pwd`/g' autogen.sh # Fix for automake 1.11
sed -i 's/AM_INIT_AUTOMAKE(\[1.12/AM_INIT_AUTOMAKE(\[1.11/g' configure.ac # Reduce min AC version for ubuntu12.04
CFLAGS="-Wno-error=cast-align  -I$openssl_path/include/" ./autogen.sh --prefix=`pwd`/out --with-pic --with-crypto-library=openssl --without-gstreamer --disable-assert --disable-gtk-doc --enable-static=no --enable-shared=yes && \
make && \
make install

cd ..
```

Publishing libnice
```shell script
[[ -z "${build_os_type}" ]] && export build_os_type=linux
[[ -z "${build_os_arch}" ]] && export build_os_arch=amd64
export prebuild_path="`pwd`/libnice-prebuild/${build_os_type}_${build_os_arch}/"

[[ ! -d libnice-prebuild ]] && git clone https://git.did.science/WolverinDEV/libnice-prebuild.git
[[ -d $prebuild_path ]] && rm -r "$prebuild_path"
mkdir $prebuild_path && \
cp -r libnice/out/include/ $prebuild_path && \
cp -r libnice/out/lib/ $prebuild_path
```

# Troubleshooting
## GLIB2.0 hangs on meson
Ninja may be too old. It should work with 1.9.0.