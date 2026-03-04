#!/bin/bash

echo "═══════════════════════════════════════════════════════════"
echo "  TeaSpeak Database UTF-8 Fix Script"
echo "═══════════════════════════════════════════════════════════"
echo

# 1. Find all .db files
echo "[1] Searching for TeaSpeak database files..."
find /root/TeaSpeak -name "*.db" -type f 2>/dev/null | while read dbfile; do
    echo "  Found: $dbfile ($(du -h "$dbfile" | cut -f1))"
done
echo

# 2. Check common locations
echo "[2] Checking common database locations..."
DB_LOCATIONS=(
    "/root/TeaSpeak/Server/Root/TeaSpeak/server/environment/TeaSpeak.db"
    "/root/TeaSpeak/Server/Root/TeaSpeak/server/environment/ts3server.db"
    "/root/TeaSpeak/TeaSpeak.db"
    "/root/.teaspeak/TeaSpeak.db"
    "$HOME/.teaspeak/TeaSpeak.db"
)

DB_FILE=""
for loc in "${DB_LOCATIONS[@]}"; do
    if [ -f "$loc" ]; then
        echo "  ✓ Found database: $loc"
        DB_FILE="$loc"
        break
    fi
done

if [ -z "$DB_FILE" ]; then
    echo "  ! Database not found in common locations"
    echo
    echo "[3] Searching entire /root directory for .db files..."
    DB_FILE=$(find /root -name "*.db" -type f 2>/dev/null | head -1)
    if [ -n "$DB_FILE" ]; then
        echo "  ✓ Found: $DB_FILE"
    else
        echo "  ✗ No database file found"
        echo
        echo "The server will create a new database on first run."
        echo "Starting server to create fresh database..."
        cd /root/TeaSpeak/Server/Root/TeaSpeak/server/environment || exit 1
        timeout 5 ./TeaSpeakServer 2>&1 | tail -20
        exit 0
    fi
fi

echo
echo "[4] Database file: $DB_FILE"
echo "   Size: $(du -h "$DB_FILE" | cut -f1)"
echo

# 3. Backup and remove database
echo "[5] Creating backup and removing old database..."
BACKUP_FILE="${DB_FILE}.backup_$(date +%Y%m%d_%H%M%S)"
cp "$DB_FILE" "$BACKUP_FILE"
echo "  ✓ Backup created: $BACKUP_FILE"

rm -f "$DB_FILE"
echo "  ✓ Old database removed: $DB_FILE"
echo

# 4. Also remove any .db-shm and .db-wal files (SQLite temp files)
rm -f "${DB_FILE}-shm" "${DB_FILE}-wal" 2>/dev/null
echo "  ✓ Removed SQLite temp files (.db-shm, .db-wal)"
echo

# 5. Test server
echo "[6] Starting TeaSpeak server with fresh database..."
cd /root/TeaSpeak/Server/Root/TeaSpeak/server/environment || exit 1

timeout 15 ./TeaSpeakServer 2>&1 | tee /tmp/teaspeak_startup.log | tail -40

echo
echo "═══════════════════════════════════════════════════════════"
echo "Full startup log saved to: /tmp/teaspeak_startup.log"
echo
if grep -q "Generating default tree" /tmp/teaspeak_startup.log; then
    echo "✓ SUCCESS: Server generated new default channels"
    echo "✓ Check the log above for any errors"
else
    echo "! Server may not have created default tree"
    echo "! Check /tmp/teaspeak_startup.log for details"
fi
echo "═══════════════════════════════════════════════════════════"
