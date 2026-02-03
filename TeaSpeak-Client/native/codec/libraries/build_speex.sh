#!/bin/bash

cd $(dirname "$0")
install_directory="$(pwd)/generated/speex/"

machine="$(uname -s)"
case "${machine}" in
    Linux*)     machine=Linux;;
#    Darwin*)    machine=Mac;;
    MINGW*)     machine=MinGW;;
    *)          machine="UNKNOWN:${machine}"
esac

if [[ ${machine} == "UNKNOWN"* ]]; then
    echo "Unknown platform ${machine}"
    exit 1
fi

cd speex
if [[ ${machine} == "Linux" ]]; then
    if [[ ! -e configure ]]; then
        echo "Generating configure file"
        ./autogen.sh
        if [[ $? -ne 0 ]]; then
            echo "Failed to generate configure file"
            exit 1
        fi
    fi
fi

windows_build="win32/VS2015/"
windows_build_type="x64"
if [[ ( ! -d build ) && ( ! -d "${windows_build}/${windows_build_type}" ) ]] || [[ "$1" == "rebuild" ]]; then
    if [[ ${machine} == "Linux" ]]; then
		if [[ -e build ]]; then
			rm -r build
		fi
		mkdir build && cd build

        export CFLAGS="-fPIC"
        ../configure --prefix="${install_directory}" --with-pic
        if [[ $? -ne 0 ]]; then
            echo "Failed to configure project!"
            exit 1
        fi

        cd ..
    elif [[ ${machine} == "MinGW" ]]; then
        #Only cleanup last shit
        if [[ -e "${windows_build}" ]]; then
        	rm -r "${windows_build}"
        fi

        mkdir -p ${windows_build}
        cp -r ../template/speex_VS2015/* ${windows_build}/
    fi
fi

if [[ -e "${install_directory}" ]]; then
    echo "deleting old install directory!"
    rm -r "${install_directory}"
    echo "rm -r '${install_directory}'"
fi

if [[ ${machine} == "MinGW" ]]; then
	saved_pwd=$(pwd)
	cd "${windows_build}"
	MSBuild.exe -p:Platform=x64 -property:Configuration=Release libspeex/libspeex.vcxproj

	if [[ $? -ne 0 ]]; then
		echo "Failed to build speex!"
		exit 1
	fi
	cd ${saved_pwd}

	mkdir -p "${install_directory}/include/speex"
	mkdir -p "${install_directory}/lib/"

	cp -r include/speex/*.h "${install_directory}/include/speex/"
	cp -r ${windows_build}/libspeex/${windows_build_type}/Release/*.lib "${install_directory}/lib/"
elif [[ ${machine} == "Linux" ]]; then
    cd build

    make -j 12
    if [[ $? -ne 0 ]]; then
        echo "Failed to build speex!"
        exit 1
    fi

    make install
    if [[ $? -ne 0 ]]; then
        echo "Failed to install speex!"
        exit 1
    fi
fi

echo "Speex build successfully"