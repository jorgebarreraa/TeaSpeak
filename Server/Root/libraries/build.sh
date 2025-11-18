#!/bin/bash

# Enter third_party/ directory
cd $(dirname $0)

export build_helper_file="../build-helpers/build_helper.sh"
build_helper_scripts="../build-helpers/libraries"

source ${build_helper_file}
[[ $build_helpers_defined -ne 1 ]] && {
    echo "Failed to include build helpers."
    exit 1
}

[[ -z "${build_os_type}" ]] && {
  echo "Missing build os type. Using \"linux\""
  export build_os_type="linux"
}

[[ -z "${build_os_arch}" ]] && {
  echo "Missing build os arch. Using \"amd64\""
  export build_os_arch="amd64"
}

begin_task "build_third_party" "Building libraries"

function exec_script() {
    name=$(echo "$1" | sed -n -E 's:^build_(.*)\.sh:\1:p')
    begin_task "build_$name" "Building $name"

    echo -e "Building library with script $color_green${1}$color_normal"
    library_path="$2" ./${1}
    if [[ $? -ne 0 ]]; then
        echo "Failed to build library $name. Status code: $?"
        exit 1
    fi

    #Log the result
    end_task "build_$name" "Finished $name"
    echo ""
}

function exec_script_external() {
    name=$(echo "$1" | sed -n -E 's:^build_(.*)\.sh:\1:p')
    begin_task "build_$name" "Building $name"

    echo -e "Building library with script $color_green${1}$color_normal"
    _prefix="library_path=\"$2\" ${*:3}"
    echo "> $_prefix ./${build_helper_scripts}/\"${1}\""
    eval $_prefix ./${build_helper_scripts}/"${1}"
    code=$?
    if [[ $code -ne 0 ]]; then
        echo "Failed to build library $name. Status code: $code"
        exit 1
    fi

    #Log the result
    end_task "build_$name" "Finished $name"
    echo ""
}

exec_script_external build_libevent.sh event
#exec_script_external build_breakpad.sh breakpad  # Skipping breakpad due to C++14 compilation issues
#exec_script_external build_cxxterminal.sh CXXTerminal "libevent_path=event"  # Skipping due to -fPIC linking issues with libevent
#exec_script build_datapipes.sh DataPipes  # Skipping DataPipes - requires WebRTC dependencies
exec_script_external build_ed25519.sh ed25519
exec_script_external build_jsoncpp.sh jsoncpp
#exec_script build_mysqlconnector.sh
#exec_script_external build_opus.sh opus  # Skipping opus - AVX2 instruction errors
exec_script_external build_protobuf.sh protobuf
exec_script_external build_spdlog.sh spdlog
exec_script_external build_stringvariable.sh StringVariable
exec_script_external build_threadpool.sh Thread-Pool
exec_script_external build_tommath.sh tommath
exec_script_external build_tomcrypt.sh tomcrypt "tommath_path=`pwd`/tommath/out/${build_os_type}_${build_os_arch}"
exec_script_external build_yamlcpp.sh yaml-cpp
exec_script_external build_jemalloc.sh jemalloc
exec_script_external build_zstd.sh zstd

#Log the result
end_task "build_third_party" "Build all libraries successfully"
