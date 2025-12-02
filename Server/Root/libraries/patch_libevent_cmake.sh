#!/bin/bash
# Patch libevent to work with CMake 3.16
# CheckLinkerFlag module was introduced in CMake 3.18

PATCH_FILE="event/cmake/AddLinkerFlags.cmake"

if [ ! -f "$PATCH_FILE" ]; then
    echo "Error: $PATCH_FILE not found"
    exit 1
fi

echo "Patching $PATCH_FILE for CMake 3.16 compatibility..."

cat > "$PATCH_FILE" << 'EOF'
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
EOF

echo "✓ Patch applied successfully"
