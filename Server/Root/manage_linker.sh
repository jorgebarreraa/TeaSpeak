#!/bin/bash
#
# Linker Management Script for TeaSpeak Compilation
# Manages ld.gold enable/disable for compatibility
#

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info() { echo -e "${GREEN}[INFO]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; }

show_status() {
    echo ""
    echo "Linker Status:"
    echo "=============="

    if [ -f "/usr/bin/ld" ]; then
        echo "Current linker: $(ld --version | head -n1)"
    fi

    echo ""
    if [ -f "/usr/bin/ld.gold" ]; then
        echo "  ✓ ld.gold is ENABLED (active)"
        echo "    Location: /usr/bin/ld.gold"
        warn "This may cause compilation issues with TeaSpeak/MySQL"
        echo ""
        echo "To disable ld.gold, run: $0 disable"
    elif [ -f "/usr/bin/NOT_USED_ld.gold" ]; then
        echo "  ✓ ld.gold is DISABLED (renamed to NOT_USED_ld.gold)"
        echo "    Location: /usr/bin/NOT_USED_ld.gold"
        info "This is the recommended configuration for TeaSpeak"
        echo ""
        echo "To re-enable ld.gold, run: $0 enable"
    else
        echo "  ✗ ld.gold is NOT INSTALLED"
        info "You're using the default system linker (recommended for TeaSpeak)"
    fi
    echo ""
}

disable_gold() {
    if [ -f "/usr/bin/ld.gold" ]; then
        info "Disabling ld.gold..."
        sudo mv /usr/bin/ld.gold /usr/bin/NOT_USED_ld.gold
        info "✓ ld.gold disabled successfully"
        info "Current linker: $(ld --version | head -n1)"
    elif [ -f "/usr/bin/NOT_USED_ld.gold" ]; then
        info "ld.gold is already disabled"
    else
        warn "ld.gold not found on the system"
    fi
}

enable_gold() {
    if [ -f "/usr/bin/NOT_USED_ld.gold" ]; then
        warn "Re-enabling ld.gold..."
        warn "Note: This may cause compilation issues with TeaSpeak/MySQL"
        sudo mv /usr/bin/NOT_USED_ld.gold /usr/bin/ld.gold
        info "✓ ld.gold enabled"
        info "Current linker: $(ld --version | head -n1)"
    elif [ -f "/usr/bin/ld.gold" ]; then
        info "ld.gold is already enabled"
    else
        error "ld.gold not found on the system"
        exit 1
    fi
}

show_help() {
    cat << EOF
Linker Management Script for TeaSpeak

Usage: $0 [command]

Commands:
    status      Show current linker status (default)
    disable     Disable ld.gold (recommended for TeaSpeak)
    enable      Re-enable ld.gold
    help        Show this help message

Background:
    ld.gold is an alternative linker that can cause issues when compiling
    certain projects like TeaSpeak and MySQL. The specific error is:

        "unsupported reloc 42 against global symbol"

    This script helps you manage ld.gold to avoid these issues.

Examples:
    $0              # Show current status
    $0 status       # Show current status
    $0 disable      # Disable ld.gold (recommended)
    $0 enable       # Re-enable ld.gold

For TeaSpeak compilation, ld.gold should be DISABLED.
The compile_teaspeak_auto.sh script does this automatically.
EOF
}

# Main logic
case "${1:-status}" in
    status)
        show_status
        ;;
    disable)
        disable_gold
        echo ""
        show_status
        ;;
    enable)
        enable_gold
        echo ""
        show_status
        ;;
    help|--help|-h)
        show_help
        ;;
    *)
        error "Unknown command: $1"
        echo ""
        show_help
        exit 1
        ;;
esac
