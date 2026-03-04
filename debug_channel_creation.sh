#!/bin/bash

echo "═══════════════════════════════════════════════════════════"
echo "  TeaSpeak Channel Creation Debug"
echo "═══════════════════════════════════════════════════════════"
echo

ENV_DIR="/root/TeaSpeak/Server/Root/TeaSpeak/server/environment"
cd "$ENV_DIR" || exit 1

# Enable core dumps
ulimit -c unlimited
echo "Core dumps enabled: $(ulimit -c)"
echo

# Set core pattern if possible
if [ -w /proc/sys/kernel/core_pattern ]; then
    echo "core.%e.%p.%t" > /proc/sys/kernel/core_pattern 2>/dev/null
    echo "Core pattern: $(cat /proc/sys/kernel/core_pattern)"
fi
echo

echo "[1] Running server to generate core dump..."
# Remove old core dumps first
rm -f core.* 2>/dev/null

timeout 10 ./TeaSpeakServer 2>&1 | tail -30
echo
echo "Server exit code: $?"
echo

echo "[2] Searching for core dump..."
CORE_FILE=$(find . -name "core.*" -mmin -1 -type f 2>/dev/null | head -1)

if [ -z "$CORE_FILE" ]; then
    echo "! No core dump found"
    echo "Checking dmesg for segfault info:"
    dmesg | grep -i "teaspeak\|segfault" | tail -5
    exit 1
fi

echo "✓ Found core dump: $CORE_FILE ($(du -h "$CORE_FILE" | cut -f1))"
echo

# Check if gdb is installed
if ! command -v gdb &> /dev/null; then
    echo "Installing gdb..."
    apt-get update -qq && apt-get install -y -qq gdb
fi

echo "[3] Extracting stack trace..."
gdb -batch \
    -ex "set pagination off" \
    -ex "thread apply all bt" \
    -ex "thread apply all bt full" \
    -ex "info registers" \
    -ex "info threads" \
    ./TeaSpeakServer "$CORE_FILE" 2>&1 | tee /tmp/channel_creation_stacktrace.txt

echo
echo "═══════════════════════════════════════════════════════════"
echo "Full stack trace saved to: /tmp/channel_creation_stacktrace.txt"
echo "═══════════════════════════════════════════════════════════"
echo
echo "Thread 1 backtrace:"
grep -A 30 "^Thread 1 " /tmp/channel_creation_stacktrace.txt | head -35
