#!/bin/bash
# TeaSpeak Command Sender
# Send a single command to a running TeaSpeak server via its terminal pipe
# Usage: ./teaspeak-command.sh <command>

if [ $# -eq 0 ]; then
    echo "Usage: $0 <command>"
    echo ""
    echo "Examples:"
    echo "  $0 help"
    echo "  $0 'shutdown now Server maintenance'"
    echo "  $0 'reload config'"
    echo "  $0 meminfo"
    exit 1
fi

COMMAND="$*"
PIPE_DIR="/tmp"

# Find the server process
SERVER_PROCESS=$(pgrep -f "TeaSpeakServer" | head -1)

if [ -z "$SERVER_PROCESS" ]; then
    echo "Error: TeaSpeak server is not running"
    exit 1
fi

PIPE_IN="${PIPE_DIR}/teaspeak_${SERVER_PROCESS}_in.term"
PIPE_OUT="${PIPE_DIR}/teaspeak_${SERVER_PROCESS}_out.term"

if [ ! -p "$PIPE_IN" ]; then
    echo "Error: Input pipe not found: $PIPE_IN"
    exit 1
fi

if [ ! -p "$PIPE_OUT" ]; then
    echo "Error: Output pipe not found: $PIPE_OUT"
    exit 1
fi

# Send command
echo "$COMMAND" > "$PIPE_IN"

# Read response with timeout
timeout 2s cat "$PIPE_OUT" 2>/dev/null || true

exit 0
