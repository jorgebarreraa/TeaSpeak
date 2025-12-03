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


cd protobuf || exit 1
# git checkout 3.5.1.1
[[ ! -d _build ]] && { mkdir _build || exit 1; }
cd _build || exit 1
cmake ../cmake/ -Dprotobuf_BUILD_TESTS=OFF -DCMAKE_CXX_FLAGS="-std=c++11 -fPIC ${CXX_FLAGS}" -DCMAKE_C_FLAGS="${C_FLAGS}" -DCMAKE_BUILD_TYPE=RelWithDebInfo ${CMAKE_OPTIONS} || exit 1
make ${CMAKE_MAKE_OPTIONS} || exit 1
make install || exit 1
#ldconfig

cd ../../
set_build_successful ${library_path}
