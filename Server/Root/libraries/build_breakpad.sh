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
# Configure with C++17 to generate proper Makefile
CXXFLAGS="-std=c++17 ${CXX_FLAGS}" CFLAGS="${C_FLAGS}" ../configure

# CRITICAL: Patch Makefile to replace all remaining c++11 with c++17
# Some rules have hardcoded -std=c++11 that ignore CXXFLAGS
sed -i 's/-std=c++11/-std=c++17/g' Makefile

# Build with C++17
make CXXFLAGS="-std=c++17 ${CXX_FLAGS}" CFLAGS="${C_FLAGS}" ${MAKE_OPTIONS}
sudo make install
