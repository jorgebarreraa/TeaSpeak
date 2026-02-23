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
    "$ENV_DIR/TeaSpeak.db"
    "$ENV_DIR/ts3server.db"
    "/root/.teaspeak/TeaSpeak.db"
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
    echo "  ! No existing database found - server will create fresh one"
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
echo "[4] Verifying SSL certificates..."
if [ -f "$CERT_DIR/server-cert.pem" ] && [ -s "$CERT_DIR/server-cert.pem" ]; then
    echo "  ✓ server-cert.pem exists ($(stat -c%s "$CERT_DIR/server-cert.pem") bytes)"
else
    echo "  ✗ server-cert.pem missing or empty!"
fi

if [ -f "$CERT_DIR/query-cert.pem" ] && [ -s "$CERT_DIR/query-cert.pem" ]; then
    echo "  ✓ query-cert.pem exists ($(stat -c%s "$CERT_DIR/query-cert.pem") bytes)"
else
    echo "  ✗ query-cert.pem missing or empty!"
fi

echo
echo "═══════════════════════════════════════════════════════════"
echo "  ✅ Server Reset Complete"
echo "═══════════════════════════════════════════════════════════"
echo
echo "Now starting TeaSpeak server with fresh database..."
echo
cd "$ENV_DIR" || exit 1
timeout 15 ./TeaSpeakServer
