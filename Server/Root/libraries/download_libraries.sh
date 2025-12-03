#!/bin/bash
set -e

echo "Downloading all TeaSpeak libraries..."
cd "$(dirname "$0")"

# Function to clone or skip if exists
clone_lib() {
    local url="$1"
    local dir="$2"
    local branch="$3"

    if [ -d "$dir" ]; then
        echo "  ✓ $dir already exists, skipping"
        return 0
    fi

    echo "  Cloning $dir..."
    if [ -n "$branch" ]; then
        git clone "$url" "$dir" --branch "$branch" --depth 1
    else
        git clone "$url" "$dir" --depth 1
    fi
}

clone_lib "https://github.com/open-source-parsers/jsoncpp.git" "jsoncpp"
clone_lib "https://git.did.science/WolverinDEV/ThreadPool.git" "Thread-Pool"
clone_lib "https://git.did.science/TeaSpeak/libraries/tomcrypt.git" "tomcrypt"
clone_lib "https://git.did.science/TeaSpeak/libraries/tommath.git" "tommath"
clone_lib "https://github.com/WolverinDEV/CXXTerminal.git" "CXXTerminal"
clone_lib "https://github.com/xiph/opus" "opus"
clone_lib "https://github.com/xiph/opusfile.git" "opusfile"
clone_lib "https://github.com/jbeder/yaml-cpp.git" "yaml-cpp"
clone_lib "https://github.com/libevent/libevent.git" "event"

# Patch libevent for CMake 3.16 compatibility
if [ -f "event/cmake/AddLinkerFlags.cmake" ]; then
    echo "  Patching libevent for CMake 3.16 compatibility..."
    cat > "event/cmake/AddLinkerFlags.cmake" << 'EOFPATCH'
# Let's make Centos7 users (cmake 3.17) happy
if (NOT CMAKE_VERSION VERSION_LESS 3.18)
	include(CheckLinkerFlag)
endif()

macro(add_linker_flags)
	foreach(flag ${ARGN})
		string(REGEX REPLACE "[-.+/:= ]" "_" _flag_esc "${flag}")

if (NOT CMAKE_VERSION VERSION_LESS 3.18)
		check_linker_flag(C "${flag}" check_c_linker_flag_${_flag_esc})
endif()

		if (check_c_linker_flag_${_flag_esc})
			set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${flag}")
			set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${flag}")
		endif()
	endforeach()
endmacro()
EOFPATCH
fi

clone_lib "https://git.did.science/TeaSpeak/libraries/spdlog.git" "spdlog"
clone_lib "https://github.com/WolverinDEV/StringVariable.git" "StringVariable"
clone_lib "https://github.com/WolverinDEV/ed25519.git" "ed25519"
clone_lib "https://chromium.googlesource.com/breakpad/breakpad" "breakpad"

# Checkout specific commit that supports C++17 (GCC 9.4.0 compatible)
# f032e4c3 fixes integer overflow in minidump.cc
if [ -d "breakpad" ]; then
    echo "  Setting breakpad to C++17-compatible commit..."
    (cd breakpad && git fetch --unshallow 2>/dev/null || true && git checkout f032e4c3 2>/dev/null || true)

    # Patch 1: Fix sign-compare warning in minidump.cc
    if [ -f "breakpad/src/processor/minidump.cc" ]; then
        echo "  Patching breakpad sign-compare issue..."
        sed -i '2098s/numeric_limits<off_t>::max()/static_cast<uint64_t>(numeric_limits<off_t>::max())/' "breakpad/src/processor/minidump.cc"
    fi

    # Patch 2: Add missing ELF constants for old systems
    if [ -f "breakpad/src/common/linux/dump_symbols.cc" ]; then
        echo "  Patching breakpad ELF constants..."
        awk '/^#include/ {last=NR} {lines[NR]=$0} END {
            for(i=1; i<=NR; i++) {
                print lines[i]
                if(i==last) {
                    print ""
                    print "// Define missing ELF constants for older systems"
                    print "#ifndef SHF_COMPRESSED"
                    print "#define SHF_COMPRESSED (1 << 11)"
                    print "#endif"
                    print ""
                    print "#ifndef ELFCOMPRESS_ZLIB"
                    print "#define ELFCOMPRESS_ZLIB 1"
                    print "#endif"
                    print ""
                    print "#ifndef ELFCOMPRESS_ZSTD"
                    print "#define ELFCOMPRESS_ZSTD 2"
                    print "#endif"
                    print ""
                    print "#ifndef EM_RISCV"
                    print "#define EM_RISCV 243"
                    print "#endif"
                }
            }
        }' "breakpad/src/common/linux/dump_symbols.cc" > "breakpad/src/common/linux/dump_symbols.cc.tmp" && \
        mv "breakpad/src/common/linux/dump_symbols.cc.tmp" "breakpad/src/common/linux/dump_symbols.cc"
    fi
fi
clone_lib "https://boringssl.googlesource.com/boringssl" "boringssl"
clone_lib "https://fuchsia.googlesource.com/third_party/protobuf" "protobuf" "v3.5.1.1"
clone_lib "https://github.com/WolverinDEV/DataPipes.git" "DataPipes"
clone_lib "https://github.com/jemalloc/jemalloc.git" "jemalloc" "dev"
clone_lib "https://git.did.science/TeaSpeak/libraries/libnice-prebuild.git" "libnice"
clone_lib "https://git.did.science/TeaSpeak/libraries/glib2.0.git" "glibc"
clone_lib "https://git.did.science/TeaSpeak/libraries/openssl-prebuild.git" "openssl-prebuild"
clone_lib "https://github.com/facebook/zstd.git" "zstd"

# Download build-helpers
cd ..
clone_lib "https://github.com/WolverinDEV/build-helpers.git" "build-helpers"

echo "✓ All libraries downloaded successfully!"
