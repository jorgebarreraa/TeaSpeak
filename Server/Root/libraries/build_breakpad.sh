cd breakpad/
if [[ ! -d src/third_party/lss ]]; then
	echo "Closing LSS into breakpad"
	git clone https://chromium.googlesource.com/linux-syscall-support src/third_party/lss

	[[ $? -ne 0 ]] && {
		echo "Failed to close LSS. Abort"
		exit 1
	}
fi

cd build
../configure
# Use C++17 for breakpad (requires C++14+ for std::make_unique)
make CXXFLAGS="-std=c++17 ${CXX_FLAGS}" CFLAGS="${C_FLAGS}" ${MAKE_OPTIONS}
sudo make install
