#!/bin/bash

# TeaSpeak Server Installation Script
# Provides 3 installation options:
# 1. Precompiled optimized binary (Release, ~18 MB) - Recommended for production
# 2. Compile from source (customizable, choose Release or Debug)
# 3. Development binary with debug symbols (~363 MB) - For debugging and development

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
INSTALL_DIR="${INSTALL_DIR:-/opt/teaspeak}"
DOWNLOAD_BASE_URL="${DOWNLOAD_URL:-https://files.teaspeak.de/releases}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

print_header() {
    echo -e "${BLUE}"
    echo "╔════════════════════════════════════════════════════════════════╗"
    echo "║          TeaSpeak Server Installation Script                  ║"
    echo "╚════════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

print_section() {
    echo -e "\n${GREEN}==>${NC} ${1}"
}

print_error() {
    echo -e "${RED}Error:${NC} ${1}" >&2
}

print_warning() {
    echo -e "${YELLOW}Warning:${NC} ${1}"
}

print_success() {
    echo -e "${GREEN}✓${NC} ${1}"
}

check_dependencies() {
    print_section "Checking dependencies..."

    local missing_deps=()

    # Check for required commands
    local required_cmds=("wget" "tar" "chmod")
    for cmd in "${required_cmds[@]}"; do
        if ! command -v "$cmd" &> /dev/null; then
            missing_deps+=("$cmd")
        fi
    done

    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_error "Missing required dependencies: ${missing_deps[*]}"
        echo "Please install them using your package manager:"
        echo "  Ubuntu/Debian: sudo apt-get install wget tar"
        echo "  CentOS/RHEL:   sudo yum install wget tar"
        exit 1
    fi

    print_success "All basic dependencies are installed"
}

check_build_dependencies() {
    print_section "Checking build dependencies..."

    local missing_deps=()
    local required_cmds=("cmake" "make" "g++" "git")

    for cmd in "${required_cmds[@]}"; do
        if ! command -v "$cmd" &> /dev/null; then
            missing_deps+=("$cmd")
        fi
    done

    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_error "Missing build dependencies: ${missing_deps[*]}"
        echo "To install on Ubuntu/Debian:"
        echo "  sudo apt-get install build-essential cmake git libssl-dev libsqlite3-dev"
        echo "To install on CentOS/RHEL:"
        echo "  sudo yum groupinstall 'Development Tools'"
        echo "  sudo yum install cmake git openssl-devel sqlite-devel"
        return 1
    fi

    print_success "All build dependencies are installed"
    return 0
}

show_menu() {
    echo -e "\n${BLUE}Select installation method:${NC}\n"
    echo "1) ${GREEN}Precompiled optimized binary${NC} (Recommended)"
    echo "   - Size: ~18 MB"
    echo "   - Best for: Production use"
    echo "   - Pros: Fast installation, optimized performance"
    echo ""
    echo "2) ${YELLOW}Compile from source${NC}"
    echo "   - Size: ~14-18 MB (Release) or ~363 MB (Debug)"
    echo "   - Best for: Custom configurations, latest code"
    echo "   - Pros: Full control, can enable debug symbols"
    echo ""
    echo "3) ${RED}Development binary with debug symbols${NC}"
    echo "   - Size: ~363 MB"
    echo "   - Best for: Development, debugging, troubleshooting"
    echo "   - Pros: Full debug information for development"
    echo ""
    echo "4) Exit"
    echo ""
}

download_precompiled() {
    print_section "Downloading precompiled optimized binary..."

    # Check if we have a local precompiled binary
    if [ -f "Server/Server/server/environment/TeaSpeakServer" ]; then
        print_warning "Local binary found, but it may contain debug symbols"
        echo "We'll strip it to create an optimized version"

        mkdir -p "$INSTALL_DIR"
        cp "Server/Server/server/environment/TeaSpeakServer" "$INSTALL_DIR/"
        strip "$INSTALL_DIR/TeaSpeakServer"

        print_success "Binary optimized and installed"
    elif [ -f "original-server-distribution/TeaSpeakServer" ]; then
        print_section "Using reference binary from original-server-distribution..."
        mkdir -p "$INSTALL_DIR"
        cp original-server-distribution/TeaSpeakServer "$INSTALL_DIR/"
        print_success "Binary copied"
    else
        print_error "No precompiled binary available"
        echo "Please choose option 2 to compile from source"
        return 1
    fi

    copy_runtime_files
    setup_scripts
    print_success "Installation complete!"
    show_post_install_info
}

compile_from_source() {
    print_section "Compiling TeaSpeak from source..."

    if ! check_build_dependencies; then
        return 1
    fi

    # Ask for build type
    echo ""
    echo "Select build type:"
    echo "1) Release (Optimized, ~14-18 MB, no debug symbols)"
    echo "2) Debug (Full debug symbols, ~363 MB)"
    echo "3) RelWithDebInfo (Optimized with debug symbols, ~363 MB)"
    echo ""
    read -p "Enter choice [1-3] (default: 1): " build_choice

    case $build_choice in
        2) BUILD_TYPE="Debug" ;;
        3) BUILD_TYPE="RelWithDebInfo" ;;
        *) BUILD_TYPE="Release" ;;
    esac

    print_section "Building with type: $BUILD_TYPE"

    cd Server/Server

    # Clean previous builds if they exist
    if [ -d "build" ]; then
        print_warning "Cleaning previous build..."
        rm -rf build
    fi

    mkdir -p build
    cd build

    print_section "Running CMake configuration..."
    cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE" || {
        print_error "CMake configuration failed"
        return 1
    }

    print_section "Compiling (this may take several minutes)..."
    make -j$(nproc) || {
        print_error "Compilation failed"
        return 1
    }

    print_success "Compilation complete!"

    # Install
    mkdir -p "$INSTALL_DIR"

    if [ -f "server/environment/TeaSpeakServer" ]; then
        cp server/environment/TeaSpeakServer "$INSTALL_DIR/"

        # Show binary size
        binary_size=$(du -h "$INSTALL_DIR/TeaSpeakServer" | cut -f1)
        print_success "Binary installed (size: $binary_size)"
    else
        print_error "Compiled binary not found"
        return 1
    fi

    cd ../../..
    copy_runtime_files
    setup_scripts
    print_success "Installation complete!"
    show_post_install_info
}

download_debug_binary() {
    print_section "Installing development binary with debug symbols..."

    print_warning "This binary is large (~363 MB) and intended for development only"
    read -p "Continue? [y/N]: " confirm

    if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
        echo "Installation cancelled"
        return 1
    fi

    if [ -f "Server/Server/server/environment/TeaSpeakServer" ]; then
        # Check if it has debug symbols
        if file "Server/Server/server/environment/TeaSpeakServer" | grep -q "not stripped"; then
            print_section "Using local debug binary..."
            mkdir -p "$INSTALL_DIR"
            cp "Server/Server/server/environment/TeaSpeakServer" "$INSTALL_DIR/"
            print_success "Debug binary installed"
        else
            print_error "Local binary doesn't have debug symbols"
            echo "Please choose option 2 and build with Debug or RelWithDebInfo"
            return 1
        fi
    else
        print_error "No debug binary available"
        echo "Please choose option 2 and build with Debug or RelWithDebInfo"
        return 1
    fi

    copy_runtime_files
    setup_scripts
    print_success "Installation complete!"
    show_post_install_info
}

copy_runtime_files() {
    print_section "Copying runtime files..."

    # Copy libraries if they exist
    if [ -d "Server/Root/TeaSpeak/libs" ]; then
        cp -r Server/Root/TeaSpeak/libs "$INSTALL_DIR/"
        print_success "Libraries copied"
    fi

    # Copy RTC library if exists
    if [ -d "Server/Root/TeaSpeak/rtclib" ]; then
        cp -r Server/Root/TeaSpeak/rtclib "$INSTALL_DIR/"
        print_success "RTC library copied"
    fi

    # Copy MusicBot libraries if they exist
    if [ -d "Server/Root/TeaSpeak/MusicBot" ]; then
        cp -r Server/Root/TeaSpeak/MusicBot "$INSTALL_DIR/"
        print_success "MusicBot libraries copied"
    fi

    # Copy resources
    if [ -d "original-server-distribution/resources" ]; then
        cp -r original-server-distribution/resources "$INSTALL_DIR/"
        print_success "Resources copied"
    fi

    # Copy geolocation data
    if [ -d "original-server-distribution/geoloc" ]; then
        cp -r original-server-distribution/geoloc "$INSTALL_DIR/"
        print_success "Geolocation data copied"
    elif [ -d "Server/Server/server/geoloc_data" ]; then
        mkdir -p "$INSTALL_DIR/geoloc"
        cp -r Server/Server/server/geoloc_data/* "$INSTALL_DIR/geoloc/"
        print_success "Geolocation data copied"
    fi

    # Copy command documentation
    if [ -d "original-server-distribution/commanddocs" ]; then
        cp -r original-server-distribution/commanddocs "$INSTALL_DIR/"
        print_success "Command documentation copied"
    fi

    # Copy certificates
    if [ -d "original-server-distribution/certs" ]; then
        cp -r original-server-distribution/certs "$INSTALL_DIR/"
        print_success "Certificates copied"
    fi
}

setup_scripts() {
    print_section "Setting up startup scripts..."

    # Copy startup scripts from original distribution if available
    if [ -d "original-server-distribution" ]; then
        for script in teastart.sh teastart_minimal.sh teastart_autorestart.sh tealoop.sh install_music.sh; do
            if [ -f "original-server-distribution/$script" ]; then
                cp "original-server-distribution/$script" "$INSTALL_DIR/"
                chmod +x "$INSTALL_DIR/$script"
            fi
        done
        print_success "Startup scripts installed"
    fi

    # Make binary executable
    chmod +x "$INSTALL_DIR/TeaSpeakServer"

    # Create logs directory
    mkdir -p "$INSTALL_DIR/logs"
}

show_post_install_info() {
    echo ""
    echo -e "${GREEN}╔════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║         Installation completed successfully!                  ║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo "Installation directory: $INSTALL_DIR"
    echo ""
    echo "To start the server:"
    echo "  cd $INSTALL_DIR"
    echo "  ./teastart_minimal.sh  # First time setup"
    echo "  ./teastart.sh start    # Start server"
    echo ""
    echo "To manage the server:"
    echo "  ./teastart.sh stop     # Stop server"
    echo "  ./teastart.sh restart  # Restart server"
    echo "  ./teastart.sh status   # Check status"
    echo ""

    # Show binary information
    if [ -f "$INSTALL_DIR/TeaSpeakServer" ]; then
        binary_size=$(du -h "$INSTALL_DIR/TeaSpeakServer" | cut -f1)
        if file "$INSTALL_DIR/TeaSpeakServer" | grep -q "not stripped"; then
            echo -e "${YELLOW}Binary info:${NC} $binary_size (with debug symbols)"
        else
            echo -e "${GREEN}Binary info:${NC} $binary_size (optimized, stripped)"
        fi
    fi
    echo ""
}

main() {
    print_header

    if [ "$EUID" -eq 0 ]; then
        print_warning "Running as root. It's recommended to run TeaSpeak as a non-root user."
    fi

    check_dependencies

    while true; do
        show_menu
        read -p "Enter your choice [1-4]: " choice

        case $choice in
            1)
                download_precompiled
                break
                ;;
            2)
                compile_from_source
                break
                ;;
            3)
                download_debug_binary
                break
                ;;
            4)
                echo "Installation cancelled"
                exit 0
                ;;
            *)
                print_error "Invalid choice. Please select 1-4."
                ;;
        esac
    done
}

# Run main function
main "$@"
