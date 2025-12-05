#!/bin/bash
#
# TeaSpeak Environment Setup and Verification Script
# This script checks and installs required dependencies
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

section() {
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

# Function to check command existence
check_command() {
    local cmd=$1
    local version_cmd=$2

    if command -v $cmd &> /dev/null; then
        if [ -n "$version_cmd" ]; then
            echo -e "  ${GREEN}✓${NC} $cmd: $($version_cmd 2>&1 | head -n1)"
        else
            echo -e "  ${GREEN}✓${NC} $cmd: $(command -v $cmd)"
        fi
        return 0
    else
        echo -e "  ${RED}✗${NC} $cmd: NOT FOUND"
        return 1
    fi
}

section "TeaSpeak Environment Setup"

# ============================================================================
# Check Operating System
# ============================================================================
section "System Information"
info "OS: $(lsb_release -d 2>/dev/null | cut -f2 || cat /etc/os-release | grep PRETTY_NAME | cut -d'=' -f2)"
info "Kernel: $(uname -r)"
info "Architecture: $(uname -m)"
info "CPU cores: $(nproc --all)"
info "Memory: $(free -h | grep Mem | awk '{print $2}')"

# ============================================================================
# Check Required Tools
# ============================================================================
section "Required Tools Check"

MISSING_TOOLS=()

# Build essentials
check_command "gcc" "gcc --version" || MISSING_TOOLS+=("gcc")
check_command "g++" "g++ --version" || MISSING_TOOLS+=("g++")
check_command "make" "make --version" || MISSING_TOOLS+=("make")
check_command "cmake" "cmake --version" || MISSING_TOOLS+=("cmake")
check_command "git" "git --version" || MISSING_TOOLS+=("git")

# Rust toolchain
check_command "cargo" "cargo --version" || MISSING_TOOLS+=("cargo")
check_command "rustc" "rustc --version" || MISSING_TOOLS+=("rustc")

# Build tools
check_command "meson" "meson --version" || MISSING_TOOLS+=("meson")
check_command "ninja" "ninja --version" || MISSING_TOOLS+=("ninja")
check_command "pkg-config" "pkg-config --version" || MISSING_TOOLS+=("pkg-config")

# Additional tools
check_command "wget" "wget --version" || MISSING_TOOLS+=("wget")
check_command "tar" "tar --version" || MISSING_TOOLS+=("tar")
check_command "autoconf" "autoconf --version" || MISSING_TOOLS+=("autoconf")

# ============================================================================
# Check Linker
# ============================================================================
section "Linker Configuration"

if [ -f "/usr/bin/ld" ]; then
    info "Current linker: $(ld --version | head -n1)"
else
    error "Linker not found!"
fi

if [ -f "/usr/bin/ld.gold" ]; then
    warn "ld.gold is present and may cause issues"
    warn "Recommendation: Disable it by running:"
    echo "    sudo mv /usr/bin/ld.gold /usr/bin/NOT_USED_ld.gold"
elif [ -f "/usr/bin/NOT_USED_ld.gold" ]; then
    info "✓ ld.gold is disabled (good)"
else
    info "✓ ld.gold not present"
fi

# ============================================================================
# Version Checks
# ============================================================================
section "Version Requirements Check"

# Check GCC version (need >= 9.x)
GCC_VERSION=$(gcc -dumpversion | cut -d. -f1)
if [ "$GCC_VERSION" -ge 9 ]; then
    info "✓ GCC version $GCC_VERSION is sufficient (>= 9 required)"
else
    error "✗ GCC version $GCC_VERSION is too old (>= 9 required)"
    MISSING_TOOLS+=("gcc-9")
fi

# Check CMake version (need >= 3.16)
CMAKE_VERSION=$(cmake --version | grep version | awk '{print $3}' | cut -d. -f1,2)
CMAKE_MAJOR=$(echo $CMAKE_VERSION | cut -d. -f1)
CMAKE_MINOR=$(echo $CMAKE_VERSION | cut -d. -f2)

if [ "$CMAKE_MAJOR" -ge 3 ] && [ "$CMAKE_MINOR" -ge 16 ]; then
    info "✓ CMake version $CMAKE_VERSION is sufficient (>= 3.16 required)"
else
    warn "CMake version $CMAKE_VERSION may be too old (>= 3.16 recommended)"
fi

# ============================================================================
# Installation Recommendations
# ============================================================================
if [ ${#MISSING_TOOLS[@]} -gt 0 ]; then
    section "Missing Dependencies"
    error "The following tools are missing or outdated:"
    for tool in "${MISSING_TOOLS[@]}"; do
        echo "  - $tool"
    done

    echo ""
    info "To install missing dependencies on Ubuntu/Debian, run:"
    echo ""
    echo "  sudo apt-get update"
    echo "  sudo apt-get install -y build-essential cmake git wget autoconf"
    echo ""
    info "To install Rust (if missing):"
    echo ""
    echo "  curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh"
    echo "  source \$HOME/.cargo/env"
    echo ""
    info "To install Meson and Ninja (if missing):"
    echo ""
    echo "  pip3 install meson ninja"
    echo ""

    exit 1
else
    section "Environment Status"
    info "✓ All required tools are installed!"
    info "✓ System is ready for TeaSpeak compilation"
    echo ""
    info "You can now run the compilation script:"
    echo ""
    echo "  ./compile_teaspeak_auto.sh [debug|nightly|optimized|stable]"
    echo ""
    info "Default build type is 'optimized' if not specified"
fi

# ============================================================================
# Environment Variables Recommendation
# ============================================================================
section "Recommended Environment Variables"

cat << 'EOF'
Add these to your ~/.bashrc or run them before compilation:

    export build_os_type=linux
    export build_os_arch=amd64
    export CMAKE_MAKE_OPTIONS="-j$(nproc --all)"

These are automatically set by compile_teaspeak_auto.sh, but you may
want to set them permanently for manual builds.
EOF

# ============================================================================
# Disk Space Check
# ============================================================================
section "Disk Space Check"

AVAILABLE_SPACE=$(df -BG . | tail -1 | awk '{print $4}' | sed 's/G//')
info "Available disk space: ${AVAILABLE_SPACE}GB"

if [ "$AVAILABLE_SPACE" -lt 10 ]; then
    warn "Less than 10GB available - you may run out of space during compilation"
else
    info "✓ Sufficient disk space available"
fi

section "Setup Complete"
