# Guía de Cambios en Submódulos para Compilación de TeaSpeak

Este documento describe los cambios realizados en los submódulos `shared` y `music` que son necesarios para compilar correctamente TeaSpeak con todas las características habilitadas.

## ⚠️ IMPORTANTE

Estos cambios ya están aplicados en el código actual, pero dado que `shared` y `music` son submódulos git separados, estos cambios **NO se guardan automáticamente** en el repositorio principal. Si clonas este proyecto desde cero, necesitarás aplicar manualmente estos cambios.

---

## Cambios en `Server/shared/` (Commit: e4ad735)

### 1. `shared/CMakeLists.txt`
**Línea ~230:** Deshabilitar archivo de test
```cmake
# src/lookup/ip.cpp  # Test file - disabled
```

**Línea ~256:** Agregar dependencias de MySQL
```cmake
target_link_libraries(TeaSpeak PUBLIC
        mysql::client::static
        /usr/lib/x86_64-linux-gnu/libz.a
        openssl::crypto::shared
        openssl::ssl::shared
)
```

### 2. Includes de `<cstdint>` para tipos uint
Agregar `#include <cstdint>` en los siguientes archivos:
- `shared/src/converters/converter.h`
- `shared/src/misc/digest.cpp`
- `shared/src/misc/digest.h`
- `shared/src/misc/strobf.h`
- `shared/src/misc/task_executor.cpp`
- `shared/src/misc/utf8.h`
- `shared/src/protocol/PacketLossCalculator.cpp`
- `shared/src/query/escape.cpp`

**Razón:** GCC 13.3.0 requiere includes explícitos de tipos estándar.

---

## Recompilación de Librerías Externas

### StringVariable
```bash
cd libraries/StringVariable/out/linux_amd64
rm -rf *
cmake ../.. -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON
make -j6 && make install
```

### libevent
```bash
cd libraries/event/_build/linux_amd64
rm -rf *
cmake ../.. -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS="-fPIC -O3" \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DEVENT__DISABLE_TESTS=ON \
  -DEVENT__DISABLE_SAMPLES=ON \
  -DEVENT__DISABLE_BENCHMARK=ON
make -j6 && make install
```

### jsoncpp
```bash
cd libraries/jsoncpp/_build/linux_amd64
rm -rf *
cmake ../.. -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-std=c++17 -fPIC" \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DJSONCPP_WITH_TESTS=OFF \
  -DJSONCPP_WITH_POST_BUILD_UNITTEST=OFF
make -j6 && sudo make install
```

---

## Cambios en `build-helpers/` Submodule

### `build-helpers/cmake/FindEd25519.cmake`
Agregar después de la línea `find_package_handle_standard_args()`:

```cmake
# Create imported target for static library
if(ed25519_LIBRARIES_STATIC AND NOT TARGET ed25519::static)
    add_library(ed25519::static STATIC IMPORTED)
    set_target_properties(ed25519::static PROPERTIES
            IMPORTED_LOCATION ${ed25519_LIBRARIES_STATIC}
            INTERFACE_INCLUDE_DIRECTORIES ${ed25519_INCLUDE_DIR}
    )
endif()

# Create imported target for shared library
if(ed25519_LIBRARIES_SHARED AND NOT TARGET ed25519::shared)
    add_library(ed25519::shared SHARED IMPORTED)
    set_target_properties(ed25519::shared PROPERTIES
            IMPORTED_LOCATION ${ed25519_LIBRARIES_SHARED}
            INTERFACE_INCLUDE_DIRECTORIES ${ed25519_INCLUDE_DIR}
    )
endif()
```

**Razón:** CMake requiere imported targets para linking moderno.

---

## ✅ Cambios YA Incluidos en el Repo Principal

Los siguientes cambios **SÍ** están guardados en el repo:

- ✅ `TeaSpeak/CMakeLists.txt:107` - MusicBot habilitado
- ✅ `TeaSpeak/server/CMakeLists.txt:189,245` - TeaMusic library habilitado
- ✅ Symlinks de OpenSSL 3.0 en `libraries/openssl-prebuild/linux_amd64/lib/`
- ✅ Documentación completa en este archivo

---

## Aplicar Todos los Cambios Rápidamente

### Opción 1: Script Automático
```bash
cd Server/Root
bash apply_submodule_fixes.sh
```

### Opción 2: Manual
1. Aplicar los cambios de CMakeLists.txt en shared
2. Agregar `#include <cstdint>` en los archivos listados
3. Recompilar las 3 librerías (StringVariable, libevent, jsoncpp)
4. Reconstruir TeaSpeak

---

## Verificar que Todo Funciona

```bash
cd Server/Root
export build_os_type=linux
export build_os_arch=amd64
bash build_teaspeak.sh stable
```

Deberías ver:
- ✅ ProviderFFMpeg.so compilado
- ✅ ProviderYT.so compilado
- ✅ libTeaMusic.so compilado
- ✅ TeaSpeakServer ejecutable generado

---

## Notas Adicionales

- **OpenSSL:** Se usa OpenSSL 3.0, no 1.1
- **GCC:** Compilado y probado con GCC 13.3.0
- **CMake:** Requiere CMake 3.8+
- **Todas las características habilitadas:** TeaMusic, MusicBot, providers de música, WebRTC

---

*Documentado: 2025-12-09*
*Autor: Claude AI Assistant*
