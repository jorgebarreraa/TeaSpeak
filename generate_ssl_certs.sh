#!/bin/bash

echo "═══════════════════════════════════════════════════════════"
echo "  TeaSpeak SSL Certificate Generator"
echo "═══════════════════════════════════════════════════════════"
echo

# Certificate directory
CERT_DIR="/root/TeaSpeak/Server/Root/TeaSpeak/server/environment/certs"

# Create certificate directory if it doesn't exist
echo "[1] Creating certificate directory..."
mkdir -p "$CERT_DIR"
echo "  ✓ Directory: $CERT_DIR"
echo

# Generate server certificates
echo "[2] Generating TeaSpeak server SSL certificates..."

# Generate private key
openssl genrsa -out "$CERT_DIR/server-key.pem" 2048 2>/dev/null
echo "  ✓ Generated: server-key.pem"

# Generate self-signed certificate (valid for 10 years)
openssl req -new -x509 -key "$CERT_DIR/server-key.pem" \
    -out "$CERT_DIR/server-cert.pem" \
    -days 3650 \
    -subj "/C=US/ST=State/L=City/O=TeaSpeak/CN=teaspeak.server" \
    2>/dev/null
echo "  ✓ Generated: server-cert.pem"

# Generate query server certificates
echo
echo "[3] Generating Query server SSL certificates..."

# Generate private key for query
openssl genrsa -out "$CERT_DIR/query-key.pem" 2048 2>/dev/null
echo "  ✓ Generated: query-key.pem"

# Generate self-signed certificate for query
openssl req -new -x509 -key "$CERT_DIR/query-key.pem" \
    -out "$CERT_DIR/query-cert.pem" \
    -days 3650 \
    -subj "/C=US/ST=State/L=City/O=TeaSpeak/CN=teaspeak.query" \
    2>/dev/null
echo "  ✓ Generated: query-cert.pem"

echo
echo "[4] Setting permissions..."
chmod 600 "$CERT_DIR"/*.pem
chmod 755 "$CERT_DIR"
echo "  ✓ Certificates secured (600 permissions)"

echo
echo "[5] Certificate summary:"
ls -lh "$CERT_DIR"

echo
echo "═══════════════════════════════════════════════════════════"
echo "  ✅ SSL Certificates Generated Successfully"
echo "═══════════════════════════════════════════════════════════"
echo
echo "Certificates location: $CERT_DIR"
echo
echo "You can now start the TeaSpeak server:"
echo "  cd /root/TeaSpeak/Server/Root/TeaSpeak/server/environment"
echo "  ./TeaSpeakServer"
