[[ -z "${build_helper_file}" ]] && {
	echo "Missing build helper file. Please define \"build_helper_file\""
	exit 1
}
source ${build_helper_file}
[[ $build_helpers_defined -ne 1 ]] && {
    echo "Failed to include build helpers."
    exit 1
}

requires_rebuild ${library_path}
[[ $? -eq 0 ]] && exit 0

[[ -z "${datapipes_webrtc}" ]] && datapipes_webrtc=1
[[ "${datapipes_webrtc}" -eq 1 ]] && _datapipes_webrtc="ON" || datapipes_webrtc="OFF"

web_cmake_flags="-DBUILD_WEBRTC=${_datapipes_webrtc}"
if [[ ${build_os_type} != "win32" && "${_datapipes_webrtc}" == "ON" ]]; then

	# -DLibevent_DIR="/mnt/d/TeaSpeak/server/libraries/event/out/linux_amd64/lib/cmake/libevent"
    glib20_dir=$(realpath "$(pwd)/glibc/linux_${build_os_arch}/")
    # shellcheck disable=SC2125
    glib20_lib_path=$(realpath "$glib20_dir/lib/"*"/")

    # TODO: pcre really required here?
    web_cmake_flags="$web_cmake_flags -DGLIB_PREBUILD_INCLUDES=\"$glib20_dir/include;$glib20_dir/include/glib-2.0/;$glib20_lib_path/glib-2.0/include/\""
    web_cmake_flags="$web_cmake_flags -DGLIB_PREBUILD_LIBRARIES=\"$glib20_lib_path/libgio-2.0.so;z;resolv;$glib20_lib_path/libgmodule-2.0.so;$glib20_lib_path/libgobject-2.0.so;$glib20_lib_path/libffi.so;$glib20_lib_path/libglib-2.0.so;pcre\""
    web_cmake_flags="$web_cmake_flags -DLIBNICE_PREBUILD_PATH=\"../libnice/linux_${build_os_arch}\""
    echo "WebRTC flags: $web_cmake_flags"
fi


_cxx_options=""
[[ ${build_os_type} != "win32" ]] && _cxx_options="-fPIC -static-libgcc -static-libstdc++"
[[ ${build_os_type} == "win32" ]] && _cxx_options="-DWIN32"

general_options="-DCMAKE_C_FLAGS=\"-fPIC\" -DCMAKE_CXX_FLAGS=\"$_cxx_options\" -DBUILD_EXAMPLES=OFF -DBUILD_STATIC=1 -DBUILD_SHARED=1 -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
#crypto_options="-DCrypto_ROOT_DIR=\"$(pwd)/boringssl/lib/\" -DCRYPTO_TYPE=\"boringssl\""
crypto_options="-DCRYPTO_TYPE=\"openssl\" -DCrypto_ROOT_DIR=\"$(pwd)/openssl-prebuild/${build_os_type}_${build_os_arch}/\""

command="cmake_build ${library_path} ${general_options} ${crypto_options} ${web_cmake_flags} ${CMAKE_OPTIONS}"
eval "$command"
check_err_exit ${library_path} "Failed to build DataPipes!"
set_build_successful ${library_path}
