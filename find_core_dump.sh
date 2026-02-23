#!/bin/bash

echo "═══════════════════════════════════════════════════════════"
echo "  Core Dump Locator & Stack Trace Extractor"
echo "═══════════════════════════════════════════════════════════"
echo

# Check kernel core pattern
echo "[1] Checking system core dump configuration..."
if [ -f /proc/sys/kernel/core_pattern ]; then
    CORE_PATTERN=$(cat /proc/sys/kernel/core_pattern)
    echo "Core pattern: $CORE_PATTERN"
else
    echo "Cannot read /proc/sys/kernel/core_pattern"
fi
echo

# Search for core dumps in common locations
echo "[2] Searching for core dumps..."
CORE_LOCATIONS=(
    "/root/TeaSpeak/Server/Root/TeaSpeak/server/environment"
    "/root/TeaSpeak"
    "/root"
    "/var/lib/systemd/coredump"
    "/tmp"
    "/"
)

FOUND_CORES=()
for location in "${CORE_LOCATIONS[@]}"; do
    if [ -d "$location" ]; then
        echo "Searching in: $location"
        # Find core files modified in last 10 minutes
        while IFS= read -r corefile; do
            if [ -f "$corefile" ]; then
                echo "  ✓ Found: $corefile ($(du -h "$corefile" | cut -f1))"
                FOUND_CORES+=("$corefile")
            fi
        done < <(find "$location" -maxdepth 2 -name "core*" -mmin -10 2>/dev/null)
    fi
done
echo

if [ ${#FOUND_CORES[@]} -eq 0 ]; then
    echo "[!] No recent core dumps found"
    echo
    echo "[3] Attempting to generate new core dump..."
    cd /root/TeaSpeak/Server/Root/TeaSpeak/server/environment || exit 1

    # Enable core dumps
    ulimit -c unlimited
    echo "ulimit -c: $(ulimit -c)"

    # Set core pattern if possible
    if [ -w /proc/sys/kernel/core_pattern ]; then
        echo "core.%e.%p.%t" > /proc/sys/kernel/core_pattern 2>/dev/null
        echo "Updated core_pattern to: $(cat /proc/sys/kernel/core_pattern)"
    fi

    # Run server and let it crash
    echo
    echo "Running server (will crash)..."
    timeout 10 ./TeaSpeakServer 2>&1 | tail -20
    echo

    # Search again
    echo "Searching for newly created core dump..."
    find . -name "core*" -mmin -1 -ls 2>/dev/null

    NEWEST_CORE=$(find . -name "core*" -mmin -1 -type f 2>/dev/null | head -1)
    if [ -n "$NEWEST_CORE" ]; then
        FOUND_CORES+=("$NEWEST_CORE")
        echo "Found new core: $NEWEST_CORE"
    fi
fi

echo
echo "═══════════════════════════════════════════════════════════"
echo "  Stack Trace Extraction"
echo "═══════════════════════════════════════════════════════════"

if [ ${#FOUND_CORES[@]} -eq 0 ]; then
    echo "[!] No core dumps available for analysis"
    echo
    echo "Troubleshooting suggestions:"
    echo "1. Check if apport is interfering:"
    echo "   systemctl status apport"
    echo "   cat /var/crash/* (if apport is active)"
    echo
    echo "2. Try disabling apport temporarily:"
    echo "   systemctl stop apport"
    echo "   Then re-run this script"
    echo
    echo "3. Check dmesg for segfault info:"
    echo "   dmesg | tail -50"
    exit 1
fi

# Use the most recent core dump
CORE_FILE="${FOUND_CORES[0]}"
echo "Analyzing: $CORE_FILE"
echo

# Check if gdb is installed
if ! command -v gdb &> /dev/null; then
    echo "[!] gdb not installed. Installing..."
    apt-get update -qq && apt-get install -y -qq gdb
fi

# Extract stack trace
echo "[4] Extracting stack trace with gdb..."
cd /root/TeaSpeak/Server/Root/TeaSpeak/server/environment || exit 1

gdb -batch -ex "thread apply all bt full" \
    -ex "info registers" \
    -ex "info threads" \
    ./TeaSpeakServer "$CORE_FILE" 2>&1 | tee /tmp/teaspeak_stacktrace.txt

echo
echo "═══════════════════════════════════════════════════════════"
echo "Stack trace saved to: /tmp/teaspeak_stacktrace.txt"
echo "═══════════════════════════════════════════════════════════"
echo
echo "Quick summary:"
head -50 /tmp/teaspeak_stacktrace.txt | grep -A 5 "Thread 1"
