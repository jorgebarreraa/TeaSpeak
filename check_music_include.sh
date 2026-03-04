#!/bin/bash

echo "=== Verificando contenido de music/include/ ==="
echo ""

echo "1. Contenido de /root/TeaSpeak/Server/Server/music/include/:"
ls -la /root/TeaSpeak/Server/Server/music/include/

echo ""
echo "2. ¿Existe el subdirectorio teaspeak?"
if [[ -d "/root/TeaSpeak/Server/Server/music/include/teaspeak" ]]; then
    echo "✓ SÍ - Contenido:"
    ls -la /root/TeaSpeak/Server/Server/music/include/teaspeak/
else
    echo "✗ NO - El subdirectorio teaspeak NO existe"
fi

echo ""
echo "3. Buscar MusicPlayer.h en todo el directorio music:"
find /root/TeaSpeak/Server/Server/music -name "MusicPlayer.h" -type f 2>/dev/null

echo ""
echo "4. Verificar commit actual del repositorio music:"
cd /root/TeaSpeak/Server/Server/music
echo "Branch actual:"
git branch
echo ""
echo "Último commit:"
git log -1 --oneline
echo ""
echo "Estado del repositorio:"
git status

echo ""
echo "5. Listar todos los archivos .h en music/include/:"
find /root/TeaSpeak/Server/Server/music/include -name "*.h" -type f 2>/dev/null
