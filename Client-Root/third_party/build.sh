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
	./${1}
	_code=$?
	if [[ $_code -ne 0 ]]; then
		echo "Failed to build library $name. Status code: $_code"
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

exec_script_external build_abseil.sh abseil-cpp
exec_script_external build_webrtc_audio_processing.sh webrtc-audio-processing "abseil_library=$(pwd)/abseil-cpp"
exec_script_external build_rnnoise.sh rnnoise
exec_script_external build_libevent.sh libevent
exec_script_external build_datapipes.sh DataPipes
exec_script_external build_tommath.sh tommath
exec_script_external build_tomcrypt.sh tomcrypt "tommath_path=`pwd`/tommath/out/${build_os_type}_${build_os_arch}"
exec_script_external build_ed25519.sh ed25519
exec_script_external build_opus.sh opus
exec_script_external build_spdlog.sh spdlog
exec_script_external build_libfvad.sh libfvad
exec_script_external build_soxr.sh soxr
exec_script_external build_portaudio.sh portaudio
exec_script_external build_unbound.sh unbound
##[[ ${build_os_type} != "win32" ]] && exec_script build_breakpad.sh #Not required for windows TeaClient
##exec_script build_soundio.sh
##exec_script build_soundio.sh

#Log the result
end_task "build_third_party" "Build all libraries successfully"
