#!/bin/bash
#
# Fix rust-webrtc Cargo.toml dependency issue
# This script fixes the corrupted slog dependency in the rust-webrtc checkout
#

set -e

echo "════════════════════════════════════════════════════════════"
echo "  Fixing rust-webrtc Cargo.toml dependencies"
echo "════════════════════════════════════════════════════════════"

# Wait for Cargo to clone the repo first
sleep 2

# Find all rust-webrtc checkouts
WEBRTC_CHECKOUTS=$(find "$HOME/.cargo/git/checkouts/" -type d -name "rust-webrtc-*" 2>/dev/null || true)

if [[ -z "$WEBRTC_CHECKOUTS" ]]; then
    echo "⚠️  No rust-webrtc checkouts found yet"
    exit 0
fi

for CHECKOUT_DIR in $WEBRTC_CHECKOUTS; do
    echo ""
    echo "Checking: $CHECKOUT_DIR"

    # Find Cargo.toml files
    CARGO_TOMLS=$(find "$CHECKOUT_DIR" -name "Cargo.toml" 2>/dev/null || true)

    for CARGO_TOML in $CARGO_TOMLS; do
        modified=false

        if grep -q "slog =" "$CARGO_TOML" 2>/dev/null; then
            echo "  Found slog dependency in: $CARGO_TOML"

            # Check if it's broken (no version specified)
            if grep -qE "^slog\s*=\s*\{\s*\}$|^slog\s*=\s*\{\s*features" "$CARGO_TOML" 2>/dev/null; then
                echo "  ⚠️  Broken slog dependency found - fixing..."

                # Backup
                cp "$CARGO_TOML" "$CARGO_TOML.backup"

                # Fix: Add version to slog dependency
                sed -i 's/^slog = {/slog = { version = "2.7",/' "$CARGO_TOML"
                modified=true

                echo "  ✓ Fixed slog dependency"
            fi
        fi

        # Fix: Add version to [dev-dependencies.slog] section if missing
        if grep -q "^\[dev-dependencies\.slog\]$" "$CARGO_TOML" 2>/dev/null; then
            # Check if version is missing
            if ! grep -A1 "^\[dev-dependencies\.slog\]$" "$CARGO_TOML" | grep -q "^version"; then
                echo "  ⚠️  Found [dev-dependencies.slog] without version - fixing..."

                # Backup if not already done
                [[ ! -f "$CARGO_TOML.backup" ]] && cp "$CARGO_TOML" "$CARGO_TOML.backup"

                # Add version line after [dev-dependencies.slog]
                sed -i '/^\[dev-dependencies\.slog\]$/a version = "2.5.2"' "$CARGO_TOML"
                modified=true

                echo "  ✓ Added version to dev-dependencies.slog"
            fi
        fi

        if [[ "$modified" == "false" ]]; then
            echo "  ✓ Cargo.toml looks OK"
        fi
    done
done

echo ""
echo "════════════════════════════════════════════════════════════"
echo "  Cargo dependencies fixed"
echo "════════════════════════════════════════════════════════════"
