#!/bin/bash
# Patch breakpad to fix sign-compare warning in minidump.cc line 2098

PATCH_FILE="breakpad/src/processor/minidump.cc"

if [ ! -f "$PATCH_FILE" ]; then
    echo "Error: $PATCH_FILE not found"
    exit 1
fi

echo "Patching $PATCH_FILE for sign-compare fix..."

# Fix line 2098: Cast numeric_limits<off_t>::max() to uint64_t to match MDRVA64 type
sed -i '2098s/numeric_limits<off_t>::max()/static_cast<uint64_t>(numeric_limits<off_t>::max())/' "$PATCH_FILE"

echo "✓ Patch applied successfully"

# Verify the change
grep -n "thread_name_rva > static_cast<uint64_t>" "$PATCH_FILE" && echo "✓ Verification passed"
