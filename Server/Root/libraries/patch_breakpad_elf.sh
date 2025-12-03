#!/bin/bash
# Patch breakpad to add missing ELF constants for old systems

PATCH_FILE="breakpad/src/common/linux/dump_symbols.cc"

if [ ! -f "$PATCH_FILE" ]; then
    echo "Error: $PATCH_FILE not found"
    exit 1
fi

echo "Patching $PATCH_FILE for missing ELF constants..."

# Create temporary file with defines
cat > /tmp/elf_defines.txt << 'EOF'

// Define missing ELF constants for older systems
#ifndef SHF_COMPRESSED
#define SHF_COMPRESSED (1 << 11)
#endif

#ifndef ELFCOMPRESS_ZLIB
#define ELFCOMPRESS_ZLIB 1
#endif

#ifndef ELFCOMPRESS_ZSTD
#define ELFCOMPRESS_ZSTD 2
#endif

#ifndef EM_RISCV
#define EM_RISCV 243
#endif

EOF

# Insert after the last #include line
awk '/^#include/ {last=NR} {lines[NR]=$0} END {
    for(i=1; i<=NR; i++) {
        print lines[i]
        if(i==last) {
            while((getline < "/tmp/elf_defines.txt") > 0) print
        }
    }
}' "$PATCH_FILE" > "$PATCH_FILE.tmp" && mv "$PATCH_FILE.tmp" "$PATCH_FILE"

rm -f /tmp/elf_defines.txt

echo "✓ Patch applied successfully"

# Verify the changes
grep -q "ifndef SHF_COMPRESSED" "$PATCH_FILE" && echo "✓ SHF_COMPRESSED defined"
grep -q "ifndef ELFCOMPRESS_ZLIB" "$PATCH_FILE" && echo "✓ ELFCOMPRESS_ZLIB defined"
grep -q "ifndef EM_RISCV" "$PATCH_FILE" && echo "✓ EM_RISCV defined"
