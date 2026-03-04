#!/bin/bash

echo "Getting crash stack trace..."
echo

# Detect script location and use correct path
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENV_DIR="$SCRIPT_DIR/Server/Root/TeaSpeak/server/environment"

echo "Using environment directory: $ENV_DIR"
cd "$ENV_DIR" || exit 1

# Enable core dumps
ulimit -c unlimited

# Remove old core dumps
rm -f core.* 2>/dev/null

# Run server briefly to get crash
timeout 5 ./TeaSpeakServer 2>&1 | tail -20

# Find core dump
CORE_FILE=$(find . -name "core.*" -mmin -1 -type f 2>/dev/null | head -1)

if [ -z "$CORE_FILE" ]; then
    echo "No core dump found"
    exit 1
fi

# Extract crash location
echo
echo "=== CRASH LOCATION ==="
gdb -batch -ex "bt 10" ./TeaSpeakServer "$CORE_FILE" 2>&1 | grep -A 10 "^#0"
