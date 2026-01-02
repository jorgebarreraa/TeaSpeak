#!/bin/bash

echo "=== Verificando estructura de submódulos ==="
echo ""

echo "1. ¿Existe el directorio music?"
if [[ -d "/root/TeaSpeak/Server/Server/music" ]]; then
    echo "✓ SÍ - /root/TeaSpeak/Server/Server/music existe"
    ls -la /root/TeaSpeak/Server/Server/music
else
    echo "✗ NO - /root/TeaSpeak/Server/Server/music NO existe"
fi

echo ""
echo "2. ¿Existe el header MusicPlayer.h?"
if [[ -f "/root/TeaSpeak/Server/Server/music/include/teaspeak/MusicPlayer.h" ]]; then
    echo "✓ SÍ - MusicPlayer.h existe"
else
    echo "✗ NO - MusicPlayer.h NO existe"
fi

echo ""
echo "3. ¿Es un repositorio git?"
if [[ -d "/root/TeaSpeak/Server/Server/music/.git" ]]; then
    echo "✓ SÍ - Es un repositorio git"
    cd /root/TeaSpeak/Server/Server/music
    git remote -v
else
    echo "✗ NO - NO es un repositorio git"
fi

echo ""
echo "4. Verificar symlink"
ls -la /root/TeaSpeak/Server/Root/TeaSpeak | grep "^l"

echo ""
echo "5. ¿Puede CMake encontrar el include?"
if [[ -f "/root/TeaSpeak/Server/Root/TeaSpeak/music/include/teaspeak/MusicPlayer.h" ]]; then
    echo "✓ SÍ - CMake puede encontrar el header vía symlink"
else
    echo "✗ NO - CMake NO puede encontrar el header"
fi
