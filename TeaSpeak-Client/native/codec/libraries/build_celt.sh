#!/bin/bash

cd $(dirname "$0")
install_directory="$(pwd)/generated/celt/"

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

cd celt

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
        if [[ -e "${windows_build}/${windows_build_type}" ]]; then
        	rm -r "${windows_build}/${windows_build_type}"
        fi
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
	MSBuild.exe -p:Platform=x64 -property:Configuration=Release opus.vcxproj
	if [[ $? -ne 0 ]]; then
		echo "Failed to build celt!"
		exit 1
	fi
	cd ${saved_pwd}

	mkdir -p "${install_directory}/include/celt"
	mkdir -p "${install_directory}/lib/"

	cp -r include/* "${install_directory}/include/celt/"
	cp -r ${windows_build}/${windows_build_type}/Release/*.lib "${install_directory}/lib/"
elif [[ ${machine} == "Linux" ]]; then
    cd build

    make -j 12
    if [[ $? -ne 0 ]]; then
        echo "Failed to build celt!"
        exit 1
    fi

    make install
    if [[ $? -ne 0 ]]; then
        echo "Failed to install celt!"
        exit 1
    fi
fi

echo "Celt build successfully"