#!/bin/bash

echo "═══════════════════════════════════════════════════════════"
echo "  TeaSpeak Segfault Diagnosis Script"
echo "═══════════════════════════════════════════════════════════"
echo

cd /root/TeaSpeak/Server/Root/TeaSpeak/server/environment || exit 1

echo "[1] Checking if TeaSpeakServer executable exists..."
if [ -f ./TeaSpeakServer ]; then
    echo "✓ TeaSpeakServer found ($(du -h TeaSpeakServer | cut -f1))"
    ls -lh TeaSpeakServer
    SERVER_BINARY="./TeaSpeakServer"
elif [ -f ./ts3 ]; then
    echo "✓ ts3 found ($(du -h ts3 | cut -f1))"
    ls -lh ts3
    SERVER_BINARY="./ts3"
else
    echo "✗ TeaSpeakServer/ts3 not found"
    ls -lh
    exit 1
fi
echo

echo "[2] Checking shared library dependencies..."
ldd $SERVER_BINARY | grep "not found" && echo "✗ Missing shared libraries!" || echo "✓ All shared libraries found"
echo
echo "Full ldd output:"
ldd $SERVER_BINARY
echo

echo "[3] Checking if libCXXTerminal.so is available..."
if [ -f "/root/TeaSpeak/Server/Root/libraries/CXXTerminal/out/linux_amd64/lib/libCXXTerminal.so" ]; then
    echo "✓ libCXXTerminal.so exists"
    ls -lh /root/TeaSpeak/Server/Root/libraries/CXXTerminal/out/linux_amd64/lib/libCXXTerminal.so
else
    echo "✗ libCXXTerminal.so not found"
fi
echo

echo "[4] Checking if libTeaMusic.so is available..."
if [ -f "/root/TeaSpeak/Server/Root/TeaSpeak/MusicBot/libs/libTeaMusic.so" ]; then
    echo "✓ libTeaMusic.so exists"
    ls -lh /root/TeaSpeak/Server/Root/TeaSpeak/MusicBot/libs/libTeaMusic.so
else
    echo "✗ libTeaMusic.so not found"
fi
echo

echo "[5] Setting up environment variables..."
export LD_LIBRARY_PATH="/root/TeaSpeak/Server/Root/libraries/CXXTerminal/out/linux_amd64/lib:/root/TeaSpeak/Server/Root/TeaSpeak/MusicBot/libs:/root/TeaSpeak/Server/Root/TeaSpeak/rtclib:$LD_LIBRARY_PATH"
echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
echo

echo "[6] Testing basic execution (without --version)..."
timeout 5 $SERVER_BINARY 2>&1 | head -20
echo
echo "Exit code: $?"
echo

echo "[7] Checking for core dumps..."
ls -lh /root/TeaSpeak/Server/Root/TeaSpeak/server/environment/core* 2>/dev/null || echo "No core dumps found in environment/"
ls -lh /root/core* 2>/dev/null || echo "No core dumps found in /root/"
ls -lh core* 2>/dev/null || echo "No core dumps found in current dir"
echo

echo "[8] Checking ulimit settings..."
ulimit -c
echo

echo "[9] Trying to enable core dumps and run again..."
ulimit -c unlimited
timeout 5 $SERVER_BINARY 2>&1 | head -20
echo

echo "[10] Checking for new core dumps..."
ls -lh core* 2>/dev/null || echo "No core dump generated"
echo

echo "═══════════════════════════════════════════════════════════"
echo "  Diagnosis complete"
echo "═══════════════════════════════════════════════════════════"
