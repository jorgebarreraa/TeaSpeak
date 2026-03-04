#!/bin/bash

echo "=== Diagnóstico de Parches de TeaSpeak ==="
echo ""

echo "1. Verificando Server/Server/CMakeLists.txt (LIBEVENT_PATH):"
if [[ -f "/root/TeaSpeak/Server/Server/CMakeLists.txt" ]]; then
    echo "   Archivo existe: ✓"
    if grep -q "event/_build/linux_amd64/lib" /root/TeaSpeak/Server/Server/CMakeLists.txt; then
        echo "   LIBEVENT_PATH corregido: ✓"
        grep "LIBEVENT_PATH" /root/TeaSpeak/Server/Server/CMakeLists.txt
    else
        echo "   LIBEVENT_PATH NO corregido: ✗"
        echo "   Valor actual:"
        grep "LIBEVENT_PATH" /root/TeaSpeak/Server/Server/CMakeLists.txt
    fi
else
    echo "   Archivo NO existe: ✗"
fi

echo ""
echo "2. Verificando music/CMakeLists.txt (include directories):"
if [[ -f "/root/TeaSpeak/Server/Root/TeaSpeak/music/CMakeLists.txt" ]]; then
    echo "   Archivo existe: ✓"

    if grep -q "Thread-Pool/src" /root/TeaSpeak/Server/Root/TeaSpeak/music/CMakeLists.txt; then
        echo "   Include Thread-Pool agregado: ✓"
    else
        echo "   Include Thread-Pool NO agregado: ✗"
    fi

    if grep -q "event/_build/linux_amd64/include" /root/TeaSpeak/Server/Root/TeaSpeak/music/CMakeLists.txt; then
        echo "   Include libevent agregado: ✓"
    else
        echo "   Include libevent NO agregado: ✗"
    fi

    echo ""
    echo "   Contenido actual de include_directories:"
    grep "include_directories" /root/TeaSpeak/Server/Root/TeaSpeak/music/CMakeLists.txt
else
    echo "   Archivo NO existe: ✗"
fi

echo ""
echo "3. Verificando si las librerías existen:"
echo -n "   libevent.a: "
if [[ -f "/root/TeaSpeak/Server/Root/libraries/event/_build/linux_amd64/lib/libevent.a" ]]; then
    echo "✓"
else
    echo "✗"
fi

echo -n "   Thread-Pool headers: "
if [[ -d "/root/TeaSpeak/Server/Root/libraries/Thread-Pool/src" ]]; then
    echo "✓"
    ls /root/TeaSpeak/Server/Root/libraries/Thread-Pool/src/*.h 2>/dev/null | head -3
else
    echo "✗"
fi

echo ""
echo "4. Verificando event2/thread.h:"
if [[ -f "/root/TeaSpeak/Server/Root/libraries/event/_build/linux_amd64/include/event2/thread.h" ]]; then
    echo "   event2/thread.h existe: ✓"
elif [[ -f "/root/TeaSpeak/Server/Root/libraries/event/include/event2/thread.h" ]]; then
    echo "   event2/thread.h existe en include/: ✓"
else
    echo "   event2/thread.h NO existe: ✗"
    echo "   Buscando en event/:"
    find /root/TeaSpeak/Server/Root/libraries/event -name "thread.h" 2>/dev/null
fi

echo ""
echo "=== Fin del diagnóstico ==="
