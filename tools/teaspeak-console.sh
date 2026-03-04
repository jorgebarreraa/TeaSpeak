#!/bin/bash
# TeaSpeak Interactive Console
# This script allows you to send commands to a running TeaSpeak server via its terminal pipe

set -e

# Find the server process and its pipes
PIPE_DIR="/tmp"
SERVER_PROCESS=$(pgrep -f "TeaSpeakServer" | head -1)

if [ -z "$SERVER_PROCESS" ]; then
    echo "Error: TeaSpeak server is not running"
    echo ""
    echo "Please start the server first:"
    echo "  ./TeaSpeakServer &"
    echo ""
    echo "Or in another terminal:"
    echo "  ./TeaSpeakServer"
    exit 1
fi

PIPE_IN="${PIPE_DIR}/teaspeak_${SERVER_PROCESS}_in.term"
PIPE_OUT="${PIPE_DIR}/teaspeak_${SERVER_PROCESS}_out.term"

# Wait a bit for pipes to be created if server just started
for i in {1..10}; do
    if [ -p "$PIPE_IN" ] && [ -p "$PIPE_OUT" ]; then
        break
    fi
    if [ $i -eq 1 ]; then
        echo "Waiting for server pipes to initialize..."
    fi
    sleep 0.5
done

if [ ! -p "$PIPE_IN" ]; then
    echo "Error: Input pipe not found: $PIPE_IN"
    echo "Server may not have initialized the terminal pipes correctly"
    echo "Make sure the server is fully started and running"
    exit 1
fi

if [ ! -p "$PIPE_OUT" ]; then
    echo "Error: Output pipe not found: $PIPE_OUT"
    echo "Server may not have initialized the terminal pipes correctly"
    exit 1
fi

echo "╔════════════════════════════════════════╗"
echo "║   TeaSpeak Interactive Console         ║"
echo "╚════════════════════════════════════════╝"
echo ""
echo "Connected to server process: $SERVER_PROCESS"
echo "Pipes: $PIPE_IN"
echo "       $PIPE_OUT"
echo ""
echo "Type 'help' for available commands"
echo "Type 'exit' or 'quit' to close console (server keeps running)"
echo "Press Ctrl+C to exit"
echo "----------------------------------------"
echo ""

# Function to read from output pipe in background
read_output() {
    while true; do
        if [ -p "$PIPE_OUT" ]; then
            while IFS= read -r line; do
                # Filter out empty responses
                if [ -n "$line" ]; then
                    echo "$line"
                fi
            done < "$PIPE_OUT"
        else
            # Pipe closed, server probably stopped
            exit 0
        fi
        sleep 0.05
    done
}

# Start reading output in background
read_output &
OUTPUT_PID=$!

# Cleanup function
cleanup() {
    echo ""
    echo "Closing console (server still running)..."
    kill $OUTPUT_PID 2>/dev/null
    wait $OUTPUT_PID 2>/dev/null
    exit 0
}

trap cleanup INT TERM EXIT

# Main input loop
while true; do
    # Check if server is still running
    if ! kill -0 $SERVER_PROCESS 2>/dev/null; then
        echo ""
        echo "Server process $SERVER_PROCESS has stopped."
        break
    fi

    # Read command from user
    read -p "> " -r command

    # Exit on empty input or special commands
    if [ -z "$command" ]; then
        continue
    fi

    if [ "$command" = "exit" ] || [ "$command" = "quit" ]; then
        break
    fi

    # Send command to pipe
    if [ -p "$PIPE_IN" ]; then
        echo "$command" > "$PIPE_IN"
        # Give server time to respond
        sleep 0.2
    else
        echo "Error: Input pipe no longer exists. Server may have stopped."
        break
    fi
done

cleanup
