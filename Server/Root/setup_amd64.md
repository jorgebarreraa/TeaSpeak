# Introduction
This instruction set works with `ubuntu:14.04`.

# Setting up build tools
## General required tools
```shell script
apt-get update && \
apt-get install -y \
  sudo autoconf git wget gettext    \
  build-essential realpath nano libcurl4-openssl-dev                      \
  libssl-dev libmysqlclient-dev libpcre3-dev
```

## gcc version 9
For ubuntu we could use the official `ubuntu-toolchain-r/test` ppa.    
You may need to install `python-software-properties`.
```shell script
sudo apt-get update && \
sudo apt-get install build-essential software-properties-common -y && \
sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y && \
sudo apt-get update && \
sudo apt-get install gcc-9 g++-9 -y && \
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 60 --slave /usr/bin/g++ g++ /usr/bin/g++-9 && \
sudo update-alternatives --install /usr/bin/cc cc /usr/bin/gcc-9 60 --slave /usr/bin/c++ c++ /usr/bin/c++-9 # THis fails for some reason
```

Alternatively we could build gcc from source using this script:
```shell script
wget https://raw.githubusercontent.com/darrenjs/howto/master/build_scripts/build_gcc_9.sh && chmod +x build_gcc_9.sh
# ATTENTION: Modify the script content (TODO: May use sed for this?)
./build_gcc_9.sh

sudo update-alternatives --install /usr/bin/gcc gcc /root/opt/gcc-9.2.0/bin/gcc 60 --slave /usr/bin/g++ g++ /root/opt/gcc-9.2.0/bin/g++ && \
sudo update-alternatives --install /usr/bin/cc cc /root/opt/gcc-9.2.0/bin/gcc 60 --slave /usr/bin/c++ c++ /root/opt/gcc-9.2.0/bin/c++
```
## CMake version 3.16
```shell script
wget https://github.com/Kitware/CMake/releases/download/v3.16.3/cmake-3.16.3.tar.gz && tar xvf cmake-3.16.3.tar.gz && \
cd cmake-3.16.3 && ./configure --parallel=16 && \
make -j16 && make install
```

## Patchelf
```shell script
wget http://nixos.org/releases/patchelf/patchelf-0.10/patchelf-0.10.tar.bz2 && \
tar xf patchelf-0.10.tar.bz2 && \
cd patchelf-0.10 && \
./configure && \
make install
```

## Go
View download:
```shell script
wget https://storage.googleapis.com/golang/go1.9.2.linux-amd64.tar.gz && \
sudo tar -xvf go1.9.2.linux-amd64.tar.gz && \
sudo mv go /usr/local && \
echo -e "export GOROOT=/usr/local/go\nexport PATH=$GOPATH/bin:$GOROOT/bin:$PATH" >> ~/.bashrc && source ~/.bashrc
```

Via PPA:
```shell script
sudo add-apt-repository ppa:gophers/archive -y  && \
sudo apt-get update && \
sudo apt-get install golang-1.11-go -y && \
sudo update-alternatives --install /usr/bin/go go /usr/lib/go-1.11/bin/go 1
```

## Git (if version is outdated)
```shell script
wget https://github.com/git/git/archive/v2.25.0.tar.gz && tar xvf v2.25.0.tar.gz && \
cd git-2.25.0 && \
make configure && \
./configure && \
make -j`nproc --all` &&  \
make install
```

## MySQL
```shell script
export openssl_path="$(realpath ~/openssl-prebuild/linux_amd64)" && \
apt-get install -y libncurses5-dev && \
git clone https://github.com/mysql/mysql-server.git && cd mysql-server \
mkdir build_ && cd build_ && \
cmake .. -DBUILD_CONFIG=mysql_release -DWITH_ZLIB=bundled -DDOWNLOAD_BOOST=1 -DWITH_BOOST=`pwd`/boost-library/ -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-lrt" -DWITH_SSL="$openssl_path" -DCMAKE_INSTALL_PREFIX=`pwd`/../out/ &&
make -j 32 # This will fail, but the client will be builded
cmake --install . # This will fail, but the client should work
```
You may have to do this before (on error `unsupported reloc 42 against global symbol`):
https://bugs.mysql.com/bug.php?id=89391
```shell script
mv /usr/bin/ld.gold /usr/bin/NOT_USED_ld.gold
```

## Setting up github
```shell script
git config --global credential.helper store && \
git config --global user.name WolverinDEV && \
git config --global user.email git@teaspeak.de
```

# Compiling required libraries
## Sqlite3 (Sometimes required if the build host is quite old)
```shell script
wget https://sqlite.org/2020/sqlite-autoconf-3310100.tar.gz && tar xvf sqlite-autoconf-3310100.tar.gz && \
cd sqlite-autoconf-3310100 && \
./configure && \
make -j 2 && \
make install
```

# Setting up TeaSpeak
## Clone the project
```shell script
git clone https://git.did.science/WolverinDEV/TeaSpeak-Parent.git server && cd server && \
git submodule update --init --recursive
```

## General env value setup
```shell script
export build_os_type=linux
export build_os_arch=amd64
export CMAKE_MAKE_OPTIONS="-j`nproc --all`"
export MAKE_OPTIONS="$CMAKE_MAKE_OPTIONS"
```

## Build libraries
```shell script
./libraries/build.sh
```

## Build TeaSpeak
```shell script
./build_teaspeak.sh release
```