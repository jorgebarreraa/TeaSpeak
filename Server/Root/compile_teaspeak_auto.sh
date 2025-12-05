#!/bin/bash
#
# TeaSpeak Automated Compilation Script
# This script handles all necessary setup and compilation steps
#

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored messages
info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Change to the script directory
cd "$(dirname "$0")"
SCRIPT_DIR="$(pwd)"

info "Starting TeaSpeak automated compilation"
info "Working directory: $SCRIPT_DIR"

# ============================================================================
# STEP 1: Setup environment variables
# ============================================================================
info "Step 1: Setting up environment variables"

export build_os_type=linux
export build_os_arch=amd64
export CMAKE_MAKE_OPTIONS="-j$(nproc --all)"
export MAKE_OPTIONS="$CMAKE_MAKE_OPTIONS"

# Set crypto library path (OpenSSL prebuild)
export crypto_library_path="$SCRIPT_DIR/libraries/openssl-prebuild/${build_os_type}_${build_os_arch}"

info "  build_os_type=$build_os_type"
info "  build_os_arch=$build_os_arch"
info "  crypto_library_path=$crypto_library_path"
info "  Parallel jobs: $(nproc --all)"

# ============================================================================
# STEP 2: Handle ld.gold linker issue
# ============================================================================
info "Step 2: Checking and handling ld.gold linker"

if [ -f "/usr/bin/ld.gold" ] && [ ! -f "/usr/bin/NOT_USED_ld.gold" ]; then
    warn "  ld.gold detected - this can cause compilation issues"
    warn "  Moving ld.gold to NOT_USED_ld.gold"
    sudo mv /usr/bin/ld.gold /usr/bin/NOT_USED_ld.gold
    info "  ✓ ld.gold disabled"
elif [ -f "/usr/bin/NOT_USED_ld.gold" ]; then
    info "  ✓ ld.gold already disabled"
else
    info "  ✓ ld.gold not present"
fi

# Verify current linker
info "  Current linker: $(ld --version | head -n1)"

# ============================================================================
# STEP 3: Download libraries (if not already done)
# ============================================================================
info "Step 3: Downloading required libraries"

if [ ! -d "libraries/jsoncpp" ] || [ ! -d "libraries/opus" ]; then
    info "  Downloading libraries..."
    cd libraries
    bash download_libraries.sh
    cd "$SCRIPT_DIR"
    info "  ✓ Libraries downloaded"
else
    info "  ✓ Libraries already present"
fi

# ============================================================================
# STEP 4: Verify required tools
# ============================================================================
info "Step 4: Verifying required tools"

# Check for required tools
REQUIRED_TOOLS="cmake gcc g++ git cargo rustc meson ninja pkg-config"
MISSING_TOOLS=""

for tool in $REQUIRED_TOOLS; do
    if ! command -v $tool &> /dev/null; then
        MISSING_TOOLS="$MISSING_TOOLS $tool"
    else
        info "  ✓ $tool: $(command -v $tool)"
    fi
done

if [ -n "$MISSING_TOOLS" ]; then
    error "Missing required tools:$MISSING_TOOLS"
    error "Please install them before continuing"
    exit 1
fi

# Check versions
info "  CMake version: $(cmake --version | head -n1)"
info "  GCC version: $(gcc --version | head -n1)"
info "  Rust version: $(rustc --version)"
info "  Meson version: $(meson --version)"

# ============================================================================
# STEP 5: Build rtclib (RTC library with glib and dependencies)
# ============================================================================
info "Step 5: Building rtclib (RTC library)"

cd "$SCRIPT_DIR/TeaSpeak/rtclib"

if [ ! -f "libteaspeak_rtc.so" ]; then
    info "  Building rtclib..."
    crypto_library_path="$crypto_library_path" bash generate_shared_library.sh

    if [ $? -eq 0 ] && [ -f "libteaspeak_rtc.so" ]; then
        info "  ✓ rtclib built successfully"
    else
        error "  Failed to build rtclib"
        exit 1
    fi
else
    info "  ✓ rtclib already built (libteaspeak_rtc.so exists)"
    read -p "  Rebuild rtclib? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        info "  Rebuilding rtclib..."
        rm -f libteaspeak_rtc.so
        crypto_library_path="$crypto_library_path" bash generate_shared_library.sh
        info "  ✓ rtclib rebuilt"
    fi
fi

cd "$SCRIPT_DIR"

# ============================================================================
# STEP 6: Build TeaSpeak Server
# ============================================================================
info "Step 6: Building TeaSpeak Server"

# Determine build type
BUILD_TYPE="${1:-optimized}"
info "  Build type: $BUILD_TYPE"

# Run the build script
info "  Running build_teaspeak.sh..."

export build_os_type
export build_os_arch
export crypto_library_path

bash build_teaspeak.sh "$BUILD_TYPE"

if [ $? -eq 0 ]; then
    info "✓ TeaSpeak compilation completed successfully!"
    info ""
    info "Build artifacts should be in: TeaSpeak/build/"

    # Show some info about the build
    if [ -d "TeaSpeak/build" ]; then
        info "Build directory contents:"
        ls -lh TeaSpeak/build/ | head -20
    fi
else
    error "TeaSpeak compilation failed!"
    exit 1
fi

# ============================================================================
# Summary
# ============================================================================
info ""
info "========================================="
info "Compilation Summary"
info "========================================="
info "Build type: $BUILD_TYPE"
info "OS: $build_os_type"
info "Architecture: $build_os_arch"
info "Linker: $(ld --version | head -n1)"
info ""
info "To run TeaSpeak server, navigate to the build directory:"
info "  cd TeaSpeak/build/"
info ""
info "To rebuild, run:"
info "  ./compile_teaspeak_auto.sh [debug|nightly|optimized|stable]"
info "========================================="
