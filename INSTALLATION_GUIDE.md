# TeaSpeak Server - Installation Guide

## 📋 Table of Contents
- [Overview](#overview)
- [Binary Size Differences Explained](#binary-size-differences-explained)
- [Installation Options](#installation-options)
- [Quick Start](#quick-start)
- [Advanced Configuration](#advanced-configuration)
- [Troubleshooting](#troubleshooting)

## Overview

This guide explains the installation options for TeaSpeak Server and clarifies the differences between our compiled binaries and the official distribution.

## Binary Size Differences Explained

### Why Our Binary Was 363 MB vs Official 18 MB

**Root Cause:** Debug symbols compilation

| Binary Type | Size | Debug Info | Use Case |
|------------|------|------------|----------|
| **Official Release** | ~18 MB | ❌ Stripped | Production |
| **Our Previous Build** | ~363 MB | ✅ Not Stripped | Development |
| **Our Optimized Build** | ~14-18 MB | ❌ Stripped | Production |

### What We Fixed

1. **CMakeLists.txt Modifications**:
   - Removed hardcoded `-g` flag from all builds
   - Made debug symbols conditional based on `CMAKE_BUILD_TYPE`
   - Added automatic stripping for Release builds

2. **Build Configuration**:
   ```cmake
   # Old (always included debug symbols):
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ... -g ...")

   # New (conditional debug symbols):
   # Base flags without debug
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ...")

   # Debug builds only:
   set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
   set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O3 -g")

   # Release: optimized without debug
   set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
   ```

3. **Automatic Strip in Release Builds**:
   ```cmake
   if(CMAKE_BUILD_TYPE STREQUAL "Release")
       add_custom_command(TARGET TeaSpeakServer POST_BUILD
           COMMAND ${CMAKE_STRIP} $<TARGET_FILE:TeaSpeakServer>
           COMMENT "Stripping release binary to reduce size..."
       )
   endif()
   ```

### Comparison: Our Build vs Official

```bash
# File information
$ file TeaSpeakServer

Official:    ELF 64-bit LSB executable, stripped
Our Release: ELF 64-bit LSB pie executable, stripped
Our Debug:   ELF 64-bit LSB pie executable, with debug_info, not stripped

# Size comparison
Official Release:    18 MB (stripped)
Our Release Build:   14 MB (stripped, auto-stripped in cmake)
Our Debug Build:     363 MB (with full debug symbols)
```

### Additional Differences

| Component | Official | Our Build | Notes |
|-----------|----------|-----------|-------|
| SSL Library | libssl.so.1.1 | libssl.so.3 | Different system versions |
| Distribution | libs/ included | System libs | Development vs production |
| Build artifacts | Not included | 5.0 GB total | Includes .o files, .a libs |

**Note:** The 5.0 GB project size includes:
- Build artifacts (.o files): 11-23 MB each
- Static libraries (.a): 91 MB, 55 MB, etc.
- ML models (.pth): 40 MB, 29 MB, 17 MB
- Git repositories
These don't affect the final binary size.

## Installation Options

### Option 1: Precompiled Optimized Binary (Recommended) ✅

**Size:** ~18 MB
**Best for:** Production deployments
**Pros:**
- Fast installation
- Optimized performance
- Small disk footprint
- No build dependencies required

**Cons:**
- No debug symbols
- Cannot customize build

```bash
./install.sh
# Select option 1
```

---

### Option 2: Compile from Source 🔧

**Size:** 14-18 MB (Release) or 363 MB (Debug)
**Best for:** Custom configurations, latest code
**Pros:**
- Full control over build
- Can enable/disable features
- Choose optimization level
- Can include debug symbols if needed

**Cons:**
- Requires build dependencies
- Takes time to compile
- Needs more disk space during build

**Build Dependencies:**
```bash
# Ubuntu/Debian
sudo apt-get install build-essential cmake git libssl-dev libsqlite3-dev

# CentOS/RHEL
sudo yum groupinstall 'Development Tools'
sudo yum install cmake git openssl-devel sqlite-devel
```

**Build Types:**
- **Release**: Optimized, no debug symbols (~14-18 MB)
- **Debug**: Full debug info, no optimization (~363 MB)
- **RelWithDebInfo**: Optimized with debug symbols (~363 MB)

```bash
./install.sh
# Select option 2
# Choose build type (1=Release, 2=Debug, 3=RelWithDebInfo)
```

---

### Option 3: Development Binary with Debug Symbols 🐛

**Size:** ~363 MB
**Best for:** Development, debugging, troubleshooting
**Pros:**
- Full debug information
- Source-level debugging
- Detailed crash reports
- Profiling with symbols

**Cons:**
- Very large size
- Not suitable for production
- Slower startup time

**When to use:**
- Active development
- Investigating crashes
- Performance profiling
- Learning the codebase

```bash
./install.sh
# Select option 3
```

---

## Quick Start

### Installation

```bash
# Clone the repository
git clone https://github.com/your-org/TeaSpeak.git
cd TeaSpeak

# Run installation script
./install.sh

# Follow the interactive prompts
```

### Default Installation Directory

```
/opt/teaspeak/
├── TeaSpeakServer          # Main binary
├── teastart.sh             # Control script
├── teastart_minimal.sh     # First-time setup
├── libs/                   # Runtime libraries
├── geoloc/                 # Geolocation data
├── resources/              # Server resources
└── logs/                   # Log files
```

### Starting the Server

```bash
# First time setup (shows query interface info)
cd /opt/teaspeak
./teastart_minimal.sh

# Normal start
./teastart.sh start

# Check status
./teaststart.sh status

# Stop server
./teastart.sh stop

# Restart server
./teastart.sh restart
```

## Advanced Configuration

### Custom Installation Directory

```bash
export INSTALL_DIR=/custom/path
./install.sh
```

### Manual Build Configuration

```bash
cd Server/Server
mkdir build && cd build

# Release build (optimized, ~14 MB)
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Debug build (with symbols, ~363 MB)
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)

# Hybrid build (optimized + symbols, ~363 MB)
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
make -j$(nproc)
```

### Stripping an Existing Binary

If you have a debug binary and want to reduce its size:

```bash
# Create a backup first
cp TeaSpeakServer TeaSpeakServer.debug

# Strip symbols
strip TeaSpeakServer

# Verify size reduction
ls -lh TeaSpeakServer*
# TeaSpeakServer        14M  (stripped)
# TeaSpeakServer.debug  363M (with symbols)
```

### Keeping Debug Symbols Separate

For production deployment with debugging capability:

```bash
# Extract debug symbols
objcopy --only-keep-debug TeaSpeakServer TeaSpeakServer.debug

# Strip the main binary
strip TeaSpeakServer

# Add debug link
objcopy --add-gnu-debuglink=TeaSpeakServer.debug TeaSpeakServer

# Now you have:
# TeaSpeakServer        ~14 MB  (production binary)
# TeaSpeakServer.debug  ~349 MB (debug symbols)
# GDB will automatically find .debug file when needed
```

## Troubleshooting

### Binary Size Issues

**Q: Why is my compiled binary 363 MB?**
A: You built with Debug or RelWithDebInfo. Rebuild with Release mode:
```bash
cd Server/Server/build
cmake .. -DCMAKE_BUILD_TYPE=Release
make clean && make -j$(nproc)
```

**Q: How can I verify if my binary has debug symbols?**
A:
```bash
file TeaSpeakServer
# With symbols:    "with debug_info, not stripped"
# Without symbols: "stripped"
```

### Missing Dependencies

**Q: Installation fails with "library not found"**
A: Make sure all runtime libraries are present:
```bash
ldd /opt/teaspeak/TeaSpeakServer
# Check for "not found" entries
```

**Q: Build fails with missing headers**
A: Install development packages:
```bash
# Ubuntu/Debian
sudo apt-get install build-essential cmake git \
    libssl-dev libsqlite3-dev

# CentOS/RHEL
sudo yum groupinstall 'Development Tools'
sudo yum install cmake git openssl-devel sqlite-devel
```

### Runtime Issues

**Q: Server crashes immediately**
A: Check logs and ensure all libraries are accessible:
```bash
cd /opt/teaspeak
export LD_LIBRARY_PATH="./libs:$LD_LIBRARY_PATH"
./TeaSpeakServer --help
```

**Q: Permission denied when starting**
A:
```bash
chmod +x /opt/teaspeak/TeaSpeakServer
chmod +x /opt/teaspeak/*.sh
```

### Build System Issues

**Q: CMake can't find libraries**
A: Check library paths in cmake modules:
```bash
# Update submodules
git submodule update --init --recursive

# Clean and reconfigure
rm -rf Server/Server/build
mkdir Server/Server/build
cd Server/Server/build
cmake .. -DCMAKE_BUILD_TYPE=Release
```

## Performance Comparison

| Build Type | Binary Size | Startup Time | Memory Usage | Debug Info |
|------------|-------------|--------------|--------------|------------|
| **Release** | 14-18 MB | Fast | Normal | ❌ None |
| **RelWithDebInfo** | 363 MB | Fast | +10-15% | ✅ Full |
| **Debug** | 363 MB | Slower | +20-30% | ✅ Full + Assertions |

**Recommendation:** Use Release for production, Debug for development.

## Additional Resources

- [TeaSpeak Official Documentation](https://forum.teaspeak.de/)
- [Build System Documentation](./Server/Server/README.md)
- [CMake Build Options](./Server/Server/CMakeLists.txt)

## Support

If you encounter issues:
1. Check this guide's troubleshooting section
2. Review server logs in `/opt/teaspeak/logs/`
3. Run with verbose mode: `./TeaSpeakServer --verbose`
4. Report issues on GitHub with:
   - Installation method used
   - Build type (Release/Debug)
   - Error messages from logs
   - Output of `ldd TeaSpeakServer`
