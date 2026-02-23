#!/bin/bash

echo "═══════════════════════════════════════════════════════════"
echo "  TeaSpeak Fresh Server Reset"
echo "═══════════════════════════════════════════════════════════"
echo

CERT_DIR="/root/TeaSpeak/Server/Root/TeaSpeak/server/environment/certs"
ENV_DIR="/root/TeaSpeak/Server/Root/TeaSpeak/server/environment"

echo "[1] Cleaning up old/empty certificate files..."
# Remove the empty certificate files created earlier
rm -f "$CERT_DIR/query_certificate.pem" "$CERT_DIR/query_privatekey.pem"
echo "  ✓ Removed empty certificate files"
echo

echo "[2] Searching for existing database file..."
DB_LOCATIONS=(
    "$ENV_DIR/TeaData.sqlite"
    "$ENV_DIR/TeaSpeak.db"
    "$ENV_DIR/ts3server.db"
    "/root/.teaspeak/TeaSpeak.db"
    "/root/.teaspeak/TeaData.sqlite"
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
    echo "  Searching for any .sqlite or .db files in environment directory..."
    DB_FILE=$(find "$ENV_DIR" -maxdepth 1 \( -name "*.sqlite" -o -name "*.db" \) -type f 2>/dev/null | head -1)
    if [ -n "$DB_FILE" ]; then
        echo "    ✓ Found: $DB_FILE ($(stat -c%s "$DB_FILE" 2>/dev/null || echo "0") bytes)"
    fi
fi

if [ -z "$DB_FILE" ]; then
    echo "  ✓ No existing database - server will create fresh one"
else
    echo
    echo "[3] Backing up and removing old database..."
    BACKUP_FILE="${DB_FILE}.backup_$(date +%Y%m%d_%H%M%S)"
    cp "$DB_FILE" "$BACKUP_FILE"
    echo "  ✓ Backup created: $BACKUP_FILE"

    rm -f "$DB_FILE"
    rm -f "${DB_FILE}-shm" "${DB_FILE}-wal" 2>/dev/null
    echo "  ✓ Old database removed"
    echo "  ℹ Server will create new database with fixed channel names"
fi

echo
echo "[4] Generating SSL certificates..."
# Run the certificate generation script
cd /root/TeaSpeak || exit 1
./generate_ssl_certs.sh

echo
echo "═══════════════════════════════════════════════════════════"
echo "  ✅ Server Reset Complete"
echo "═══════════════════════════════════════════════════════════"
echo
echo "Now starting TeaSpeak server with fresh database and SSL certificates..."
echo
cd "$ENV_DIR" || exit 1
timeout 15 ./TeaSpeakServer
