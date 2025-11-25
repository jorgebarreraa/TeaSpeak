#!/bin/bash
# Script para corregir el problema de string_view con JsonCpp
# Este script modifica WebAPI.cpp para usar std::string explícitamente

TARGET_FILE="/root/teaspeak-build/server/Server/Server/license/server/WebAPI.cpp"
BACKUP_FILE="${TARGET_FILE}.backup_$(date +%Y%m%d_%H%M%S)"

echo "=== Fix JsonCpp string_view Compatibility ==="
echo "Target file: $TARGET_FILE"
echo ""

# Verificar que el archivo existe
if [ ! -f "$TARGET_FILE" ]; then
    echo "ERROR: File not found: $TARGET_FILE"
    echo "Please verify the build path is correct"
    exit 1
fi

# Hacer backup
echo "Creating backup: $BACKUP_FILE"
cp "$TARGET_FILE" "$BACKUP_FILE"

# Aplicar fixes
echo "Applying fixes..."

# Solución: Reemplazar accesos a Json::Value con literales de cadena
# Convertir message["key"] a message[std::string("key")]
# Convertir response["key"] a response[std::string("key")]

sed -i 's/message\["\([^"]*\)"\]/message[std::string("\1")]/g' "$TARGET_FILE"
sed -i 's/response\["\([^"]*\)"\]/response[std::string("\1")]/g' "$TARGET_FILE"
sed -i 's/indexed_data\["\([^"]*\)"\]/indexed_data[std::string("\1")]/g' "$TARGET_FILE"
sed -i 's/history_data\["\([^"]*\)"\]/history_data[std::string("\1")]/g' "$TARGET_FILE"
sed -i 's/builder\["\([^"]*\)"\]/builder[std::string("\1")]/g' "$TARGET_FILE"

echo "Fixes applied successfully!"
echo ""
echo "Changed patterns:"
echo "  - message[\"...\"] → message[std::string(\"...\")]"
echo "  - response[\"...\"] → response[std::string(\"...\")]"
echo "  - indexed_data[\"...\"] → indexed_data[std::string(\"...\")]"
echo "  - history_data[\"...\"] → history_data[std::string(\"...\")]"
echo "  - builder[\"...\"] → builder[std::string(\"...\")]"
echo ""
echo "Backup saved at: $BACKUP_FILE"
echo ""
echo "Next steps:"
echo "  1. Review the changes: diff $BACKUP_FILE $TARGET_FILE"
echo "  2. Rebuild: cd /root/teaspeak-build/server/Server/Server/build && make -j4"
echo ""
