# 📋 Registro de Cambios por Sesión - TeaSpeak

Este archivo documenta TODOS los cambios realizados en cada sesión para facilitar la continuidad cuando se resetea el contexto.

---

## 🔄 SESIÓN: 2026-02-18 (Rama: claude/fix-install-script-ULBSP)

### ✅ Cambios Completados:

#### 1. Fix: FindThreadPool.cmake crea target INTERFACE en lugar de STATIC IMPORTED
- **Archivo:**
  - `Server/Server/cmake/Modules/FindThreadPool.cmake`
- **Problema:**
  - `FindThreadPool.cmake` creaba `threadpool::static` como `INTERFACE IMPORTED`
  - Un target INTERFACE solo proporciona directorios de include, NO enlaza ninguna biblioteca
  - Thread-Pool (jorgebarreraa/Thread-Pool.git) NO es header-only: `threads::ThreadPool`,
    `threads::timer`, `threads::impl::ThreadBase`, `threads::impl::FutureHandleData`, etc.
    tienen sus implementaciones compiladas en `libThreadPoolStatic.a`
  - El comentario en el archivo decía "header-only, no library to link" — incorrecto
  - `libThreadPoolStatic.a` no aparecía en el comando final de linkeo → ~80 errores:
    - `undefined reference to threads::ThreadPool::execute(...)`
    - `undefined reference to threads::timer::timer(...)`
    - `undefined reference to threads::impl::FutureHandleData::triggerWaiters(...)`
    - `undefined reference to threads::impl::ThreadBase::start(...)`
    - (muchos más del mismo tipo)
- **Solución:**
  - Agrega búsqueda de `libThreadPoolStatic.a` con `find_library()` en paths paralelos
    a los paths de include (mismo orden de prioridad)
  - Si se encuentra la biblioteca: crea `threadpool::static` como `STATIC IMPORTED`
    con `IMPORTED_LOCATION` y `INTERFACE_INCLUDE_DIRECTORIES`
  - Si NO se encuentra: mantiene fallback a `INTERFACE` target (compatible con
    instalaciones que solo tienen headers)
  - `NAMES libThreadPoolStatic.a ThreadPoolStatic` — el nombre con `.` fuerza búsqueda
    de filename exacto en CMake (sin añadir prefijos/sufijos automáticos)
- **Estado:** ✅ COMPLETADO

#### 2. Fix: FindCXXTerminal.cmake crea STATIC IMPORTED para un .so → sin rpath → error en runtime
- **Archivo:**
  - `Server/Server/cmake/Modules/FindCXXTerminal.cmake`
- **Problema:**
  - `libCXXTerminal.so` es una biblioteca dinámica compartida (`.so`), NO estática
  - `FindCXXTerminal.cmake` usaba `add_library(CXXTerminal::static STATIC IMPORTED)`
  - CMake solo agrega el directorio de una biblioteca al rpath del binario cuando el
    target es `SHARED IMPORTED`. Para `STATIC IMPORTED`, asume que el código está
    integrado y no necesita rpath.
  - Resultado: el binario `TeaSpeakServer` compilaba correctamente (el linker encontraba
    el `.so` por ruta absoluta) pero al ejecutarse fallaba:
    ```
    ./TeaSpeakServer: error while loading shared libraries: libCXXTerminal.so:
    cannot open shared object file: No such file or directory
    ```
  - El rpath del binario solo tenía `MusicBot/libs` y `rtclib`, nunca el directorio
    de CXXTerminal (`libraries/CXXTerminal/out/linux_amd64/lib/`)
- **Solución:**
  - Agrega detección del tipo real de biblioteca usando regex `\.so(\.[0-9]+)*$`
  - Si la biblioteca encontrada es `.so` → `SHARED IMPORTED` (cmake agrega rpath auto)
  - Si la biblioteca encontrada es `.a` → `STATIC IMPORTED` (sin rpath, correcto)
  - Esta lógica maneja correctamente cualquier futura versión estática también
- **Estado:** ✅ COMPLETADO

#### 3. Fix: build_teaspeak.sh no construye ni ejecuta PermHelper → resources/permissions.template faltante
- **Archivo:**
  - `Server/Root/build_teaspeak.sh`
- **Problema:**
  - El servidor fallaba en runtime con:
    ```
    [CRITICAL] GLOBL | Could not open default permissions file resources/permissions.template
    [CRITICAL] GLOBL | Could not setup server instance! Stopping...
    ```
  - `PermHelper` (compilado desde `helpers/permgen.cpp`) genera este archivo leyendo
    `../helpers/server_groups` y `../helpers/channel_groups` y escribiendo `permissions.template`
  - `build_teaspeak.sh` solo construía `ProviderFFMpeg`, `ProviderYT` y `TeaSpeakServer`,
    nunca `PermHelper` y nunca lo ejecutaba
- **Solución:**
  - Agrega paso en `build_teaspeak.sh` después de la construcción de TeaSpeakServer:
    1. `cmake --build . --target PermHelper` para compilar el binario
    2. `mkdir -p ../server/environment/resources` para asegurar que el directorio existe
    3. Ejecutar PermHelper desde `../server/environment/resources/` para que las rutas
       relativas (`../helpers/`) resuelvan correctamente a `server/helpers/`
    4. El archivo generado queda en `server/environment/resources/permissions.template`
  - El paso es no-fatal (fallo en `⚠`) para no bloquear el proceso si la generación falla
- **Estado:** ✅ COMPLETADO

### 📝 Contexto de la sesión 2026-02-18:
- **TeaSpeakServer compiló exitosamente** al 100% tras los fixes de Thread-Pool y jsoncpp_lib
- Error final era de runtime (no de compilación): `libCXXTerminal.so` no encontrado
- Causa sistemática: STATIC vs SHARED IMPORTED es la distinción que controla si cmake
  agrega automáticamente la ruta de la biblioteca al rpath del binario
- El patrón de este bug puede afectar a cualquier otro `.so` declarado como STATIC IMPORTED

---



## 🔄 SESIÓN: 2026-02-17 (Rama: claude/fix-install-script-ULBSP)

### ✅ Cambios Completados:

#### 1. Fix: Módulos Find*.cmake sin targets IMPORTED (ed25519, opus, jemalloc, breakpad)
- **Archivos:**
  - `Server/Server/cmake/Modules/FindEd25519.cmake`
  - `Server/Server/cmake/Modules/FindOpus.cmake`
  - `Server/Server/cmake/Modules/FindJemalloc.cmake`
  - `Server/Server/cmake/Modules/FindBreakpad.cmake`
- **Problema:**
  - Estos módulos Find*.cmake solo definían variables pero no creaban targets IMPORTED
  - El submódulo git `shared/` (TeaSpeakLibrary) usa `target_link_libraries(... ed25519::static opus::static jemalloc::shared breakpad::static ...)`
  - CMake requiere que targets con `::` sean targets reales registrados (no solo variables)
  - Error: `CMake Error: Target "TeaSpeak" links to: ed25519::static but the target was not found`
- **Solución:**
  - `FindEd25519.cmake`: Agrega target `ed25519::static` STATIC IMPORTED
  - `FindOpus.cmake`: Agrega target `opus::static` STATIC IMPORTED
  - `FindJemalloc.cmake`: Agrega targets `jemalloc::static` y `jemalloc::shared` STATIC IMPORTED
  - `FindBreakpad.cmake`: Agrega target `breakpad::static` STATIC IMPORTED (solo si Breakpad está disponible)
- **Commit:** `e4f2c5f3`
- **Estado:** ✅ COMPLETADO

#### 2. Fix: Thread-Pool library no compilada / nombre de librería incorrecto
- **Archivos:**
  - `Server/Root/build-helpers/cmake/config/tearoot-server.cmake`
  - `apply_compilation_patches.sh` (PARCHE 44 agregado)
- **Problema:**
  - `tearoot-server.cmake` seteaba `LIBRARY_PATH_THREAD_POOL` apuntando a `libthread_pool.a`
  - El CMakeLists.txt del proyecto Thread-Pool instala la biblioteca estática como `libThreadPoolStatic.a` (no `libthread_pool.a`)
  - El directorio `libraries/Thread-Pool/` no estaba construido en la máquina de build
  - Error: `No rule to make target '.../Thread-Pool/out/linux_amd64/lib/libthread_pool.a'`
- **Solución:**
  - `tearoot-server.cmake` línea 68: `libthread_pool.a` → `libThreadPoolStatic.a`
  - PARCHE 44 en `apply_compilation_patches.sh`: Clona Thread-Pool desde GitHub y lo compila con CMake si `libThreadPoolStatic.a` no existe
  - Thread-Pool se clona desde `https://github.com/jorgebarreraa/Thread-Pool.git`
  - Se instala a `libraries/Thread-Pool/out/linux_amd64/` con headers en `include/ThreadPool/`
- **Estado:** ✅ COMPLETADO

#### 3. Fix: music/CMakeLists.txt usa path incorrecto para libevent
- **Archivos:**
  - `Server/Server/music/CMakeLists.txt`
  - `apply_compilation_patches.sh` (PARCHE 1 actualizado)
- **Problema:**
  - `music/CMakeLists.txt` referenciaba libevent en `event/_build/linux_amd64/`
  - PARCHE 43 instala libevent en `event/out/linux_amd64/` (no en `_build/`)
  - Error: `No rule to make target '.../event/_build/linux_amd64/lib/libevent.a'`
- **Solución:**
  - Cambiado `event/_build/linux_amd64/` → `event/out/linux_amd64/` en include_directories y get_filename_component para LIBEVENT_LIB y LIBEVENT_PTHREADS_LIB
  - También actualizado el check de verificación del PARCHE 1
- **Estado:** ✅ COMPLETADO

#### 5. Fix: FindThreadPool.cmake no busca en libraries/Thread-Pool/ (capital T-P)
- **Archivos:**
  - `Server/Server/cmake/Modules/FindThreadPool.cmake`
- **Problema:**
  - `FindThreadPool.cmake` busca en `libraries/threadpool/` (minúsculas, sin guión)
  - PARCHE 44 construye Thread-Pool en `libraries/Thread-Pool/out/linux_amd64/include/ThreadPool/`
  - Fallback usa `shared/src` (tiene `misc/task_executor.h` pero NO `ThreadPool/ThreadPool.h`)
  - Error: `fatal error: ThreadPool/ThreadPool.h: No such file or directory` en SqlQuery.h
- **Solución:**
  - Agrega `${CMAKE_SOURCE_DIR}/../libraries/Thread-Pool/out/.../include` como primer path de búsqueda
  - Ahora `find_path` encuentra `ThreadPool/Timer.h` en el Thread-Pool construido antes que en `shared/src`
- **Estado:** ✅ COMPLETADO

#### 4. Fix: jsoncpp static library faltante para ProviderYT
- **Archivos:**
  - `apply_compilation_patches.sh` (PARCHE 45 agregado)
- **Problema:**
  - `tearoot-server.cmake` referencia `LIBRARY_PATH_JSON = libraries/jsoncpp/out/linux_amd64/lib/libjsoncpp.a`
  - PARCHE 24 compila jsoncpp como **shared** (`-DBUILD_SHARED_LIBS=ON`) y lo instala en `/usr/local/`
  - El archivo `libjsoncpp.a` (estático) nunca se crea en `libraries/jsoncpp/out/linux_amd64/`
  - Error: `No rule to make target '.../jsoncpp/out/linux_amd64/lib/libjsoncpp.a'` al enlazar `ProviderYT.so`
- **Solución:**
  - PARCHE 45 en `apply_compilation_patches.sh`: Clona jsoncpp y lo compila como biblioteca ESTÁTICA
  - Flags: `-DBUILD_SHARED_LIBS=OFF -DCMAKE_CXX_FLAGS="-fPIC -std=c++11"`
  - Instala a `libraries/jsoncpp/out/linux_amd64/`
  - Fallback: Si jsoncpp instala como `libjsoncpp_static.a`, crea symlink a `libjsoncpp.a`
- **Estado:** ✅ COMPLETADO

#### 6. Fix: jsoncpp_lib CMake target no definido → error de linker "-ljsoncpp_lib"
- **Archivo:**
  - `Server/Root/build-helpers/cmake/config/tearoot-server.cmake`
- **Problema:**
  - `server/CMakeLists.txt` (línea 267), `license/CMakeLists.txt` (línea 68), y `shared/CMakeLists.txt`
    usan `jsoncpp_lib` como nombre de target CMake en `target_link_libraries()`
  - `find_package(jsoncpp)` falla silenciosamente si el cmake config no existe en el path esperado
  - Sin ese cmake config, el target `jsoncpp_lib` nunca se crea como IMPORTED target
  - CMake degrada el nombre desconocido a un flag de linker raw: `-ljsoncpp_lib`
  - Error: `/usr/bin/ld: cannot find -ljsoncpp_lib: No such file or directory`
  - El servidor C++ compilaba al 100% pero fallaba en el link final con este único error
- **Solución:**
  - Agrega bloque `if(NOT TARGET jsoncpp_lib)` en `tearoot-server.cmake` después de las
    variables `jsoncpp_DIR` ya existentes
  - Crea explícitamente `jsoncpp_lib` como `STATIC IMPORTED GLOBAL` con:
    - `IMPORTED_LOCATION` apuntando a `libraries/jsoncpp/${BUILD_OUTPUT}/lib/libjsoncpp.a`
    - `INTERFACE_INCLUDE_DIRECTORIES` apuntando a `libraries/jsoncpp/${BUILD_OUTPUT}/include`
  - Sigue el mismo patrón usado en todos los otros Find*.cmake del proyecto
  - `tearoot-server.cmake` se carga en el top-level antes de todos los subdirectorios
    → el target queda disponible para server/, license/ y shared/ al mismo tiempo
- **Estado:** ✅ COMPLETADO

### 📝 Contexto de la sesión 2026-02-17:
- CMake configuration pasó exitosamente después de los fixes de targets IMPORTED de la sesión anterior
- El error Thread-Pool apareció en la fase de compilación (make), no en cmake configure
- Thread-Pool NO es header-only: tiene código fuente en `src/` y crea `libThreadPoolStatic.a`
- `music/include/teaspeak/MusicPlayer.h` incluye `<ThreadPool/Future.h>` → necesita los headers instalados
- Se usa el proyecto del usuario (`jorgebarreraa/Thread-Pool.git`) para el clone de fallback
- libevent se instala a `out/linux_amd64/` (por PARCHE 43), no a `_build/linux_amd64/`
- El servidor llegó a compilar al 100% pero fallaba el link final por jsoncpp_lib (fix 6 de esta sesión)

---

## 🔄 SESIÓN: 2026-02-16 (Rama: claude/fix-install-script-ULBSP)

### ✅ Cambios Completados:

#### 1. Fix CRÍTICO: FindTomMath.cmake crea target con nombre incorrecto
- **Archivos:**
  - `Server/Root/TeaSpeak/cmake/Modules/FindTomMath.cmake`
  - `Server/Server/cmake/Modules/FindTomMath.cmake`
- **Problema:**
  - `FindTomMath.cmake` creaba el target `TomMath::static` (CamelCase)
  - `shared/CMakeLists.txt` (líneas 206, 261) y `server/CMakeLists.txt` (líneas 264, 403) referencian `tommath::static` (minúsculas)
  - Los nombres de targets CMake son case-sensitive → `TomMath::static` ≠ `tommath::static`
  - Error: `CMake Error: Target "TeaSpeak" links to: tommath::static but the target was not found`
  - Comparación: `FindTomCrypt.cmake` correctamente creaba `tomcrypt::static` (minúsculas)
- **Solución:**
  - Cambiado `TomMath::static` → `tommath::static` en ambos archivos FindTomMath.cmake
  - Ahora consistente con el patrón de `FindTomCrypt.cmake`
- **Estado:** ✅ COMPLETADO

### 📝 Contexto de la sesión 2026-02-16:
- Error detectado después de que Rust (teaspeak-webrtc) compiló exitosamente
- La compilación de Rust completó en ~1m 40s con solo warnings (no errores)
- El error era en la fase CMake Configure del servidor C++
- Solo afectaba a `tommath::static`, no a `tomcrypt::static` (ese ya estaba correcto)

---

## 🔄 SESIÓN: 2026-02-14 (Rama: claude/fix-install-script-ULBSP)

### ✅ Cambios Completados:

#### 1. Fix: Crear tearoot-server.cmake (archivo CMake crítico faltante)
- **Archivo:** `Server/Root/build-helpers/cmake/config/tearoot-server.cmake` (NUEVO)
- **Problema:**
  - `build_teaspeak.sh` pasa `-DBUILD_INCLUDE_FILE=".../cmake/config/tearoot-server.cmake"` a CMake
  - El archivo no existía → CMake fallaba con "cannot find file" → build imposible
  - Sin este archivo, `find_package(TomMath)`, etc. no tenían ROOT_DIR configurado
- **Solución:**
  - Creado `build-helpers/cmake/config/tearoot-server.cmake`
  - Incluye `tearoot-helper.cmake` para inicializar BUILD_OUTPUT = "out/linux_amd64"
  - Establece TomMath_ROOT_DIR, TomCrypt_ROOT_DIR, spdlog_ROOT_DIR, yaml-cpp_DIR, etc.
  - Establece legacy LIBRARY_PATH_* para targets Qt (LicenseManager, deshabilitado)
- **Estado:** ✅ COMPLETADO

#### 2. Fix: Crear módulos Find*.cmake faltantes
- **Archivos nuevos en `Server/Server/cmake/Modules/`:**
  - `FindTomMath.cmake` - busca en libraries/tommath/out/linux_amd64/
  - `FindTomCrypt.cmake` - busca en libraries/tomcrypt/out/linux_amd64/
  - `FindStringVariable.cmake` - busca en libraries/StringVariable/out/linux_amd64/
  - `FindDataPipes.cmake` - crea targets DataPipes::core::static y DataPipes::core::shared
  - `FindCXXTerminal.cmake` - crea target CXXTerminal::static
  - `FindBreakpad.cmake` - busca en libraries/breakpad/out/linux_amd64/
  - `FindOpus.cmake` - busca en libraries/opus/out/linux_amd64/
  - `FindJemalloc.cmake` - busca en libraries/jemalloc/out/linux_amd64/
  - `FindCrypto.cmake` - BoringSSL static (crea openssl::ssl::static, openssl::crypto::static, shared alias)
- **Problema:** `find_package(TomMath REQUIRED)` y otros fallaban sin módulos de búsqueda
- **Razón de ausencia:** Al agregar scripts a build-helpers/libraries/ en git (e1f14f41), el clone
  de jorgebarreraa/build-helpers se saltó (que hubiera traído los cmake modules también)
- **Estado:** ✅ COMPLETADO

#### 3. Fix CRÍTICO: PATCH 21 causaba exit 1 en apply_compilation_patches.sh
- **Archivo:** `apply_compilation_patches.sh` (PATCH 21 y 22)
- **Problema:**
  - PATCH 21 buscaba `_crypto_type="boringssl"` en build_datapipes.sh
  - El archivo actual NO tiene esa variable → el grep final fallaba → `exit 1`
  - Toda la compilación se interrumpía en este punto
  - PATCH 22 usaba marcador `.build_linux_amd64.txt` en vez de `.build_successful`
- **Solución:**
  - PATCH 21: solo verifica que BoringSSL está referenciado (no cambia nada)
  - PATCH 22: solo verifica y limpia marcadores, NO intenta compilar
- **Estado:** ✅ COMPLETADO

#### 4. Fix: TomMath regression en cache cleanup (commit eb6c87dd)
- **Problema:** Cache cleanup eliminaba directorios output pero no los marcadores `.build_successful`
- **Solución:** Limpieza de markers durante cleanup de cache
- **Estado:** ✅ PUSHEADO

#### 5. Fix: 4 fallos de compilación de librerías (commit 1e04d61c)
- **BoringSSL**: versiones nuevas tienen libs en build/ directamente
- **zstd**: cleanup eliminaba fuente (build/cmake), no artefactos
- **breakpad**: Requiere C++20, eliminado `-std=c++11`
- **DataPipes**: fallo en cascada con BoringSSL

#### 6. Fix: Scripts de build-helpers/libraries/ en git (commit e1f14f41)
- 16 scripts agregados al repositorio (antes externos)
- Repo ahora es autocontenido sin clonar build-helpers

#### 7. Fix: download_libraries_custom.sh (commit fb21b310)
- download_libraries.sh eliminado pero referenciado en 5 lugares
- Actualizadas todas las referencias

### 📝 Contexto de la sesión 2026-02-14:
- **Causa raíz de los problemas**: Los Find*.cmake y tearoot-server.cmake son archivos del repositorio original
  `build-helpers` de WolverinDEV. Al hacer el repo autocontenido, se incluyeron los scripts de build
  (.sh) pero no los archivos cmake. Esta sesión completa esa parte faltante.
- **IMPORTANTE**: El build completo requiere primero ejecutar `download_libraries_custom.sh` para
  clonar las fuentes, luego `build.sh` para compilar, luego `build_teaspeak.sh` para el servidor.

---

## 🔄 SESIÓN: 2025-02-08 (Rama: claude/fix-install-script-ULBSP)

### ✅ Cambios Completados:

#### 1. Fix: PATCH 36 - Shared Library OpenSSL Linking
- **Commit:** `0478ee7`
- **Archivos:**
  - `apply_compilation_patches.sh` (PATCH 36 agregado)
  - `Server/Server/shared/CMakeLists.txt` (será modificado por el patch)
- **Problema:**
  - `shared/CMakeLists.txt` enlaza OpenSSL shared libraries (`.so`)
  - Server enlaza OpenSSL static libraries (`.a`)
  - Conflicto durante linking final: ambas versiones intentan enlazarse
  - Error: `undefined reference to EVP_idea_cbc@OPENSSL_3.0.0` y otros símbolos
  - `libssl.so.3` no puede encontrar símbolos en `libcrypto.so.3`
- **Solución:**
  - PATCH 36 modifica línea 221 de `shared/CMakeLists.txt`
  - Cambio: `openssl::ssl::shared openssl::crypto::shared` → `openssl::ssl::static openssl::crypto::static`
  - Fuerza recompilación del módulo shared automáticamente
- **Estado:** ✅ PUSHEADO

### 📋 Resumen Técnico del Fix:

**Problema raíz:** El módulo shared enlazaba OpenSSL compartido mientras el server enlazaba OpenSSL estático

**Solución:**
- ✅ **PATCH 36**: shared/CMakeLists.txt usa bibliotecas estáticas de OpenSSL
- Consistencia total: todos los módulos usan OpenSSL estático
- Evita conflictos de símbolos entre versiones dinámicas y estáticas

---

## 🔄 SESIÓN: 2025-02-07 (Rama: claude/fix-install-script-ULBSP)

### ✅ Cambios Completados y Pusheados:

#### 1. Fix: PATCH 35 - OpenSSL Linking Conflicts
- **Commit:** `5631b92`
- **Archivos:**
  - `apply_compilation_patches.sh` (PATCH 35 agregado)
  - `Server/Root/libraries/DataPipes/cmake/modules/FindCrypto.cmake` (modificado)
- **Problema:**
  - DataPipes compilado con OpenSSL shared libraries (`.so`)
  - Server intentaba enlazar con OpenSSL static libraries (`.a`)
  - Conflicto de símbolos @OPENSSL_3.0.0 al mezclar static + shared
  - Error: `undefined reference to SSL_CTX_set_tlsext_servername_callback`
- **Solución:**
  - PATCH 35 modifica `FindCrypto.cmake` para priorizar bibliotecas estáticas
  - Cambio: `NAMES libssl.so` → `NAMES libssl.a ssl.lib libssl.so`
  - Cambio: `NAMES libcrypto.so` → `NAMES libcrypto.a crypto.lib libcrypto.so`
  - Fuerza recompilación de DataPipes automáticamente
- **Estado:** ✅ PUSHEADO

#### 2. Fix: PATCH 20 Corregido - OpenSSL 1.1 vs 3.0
- **Commit:** Pendiente
- **Archivo:** `apply_compilation_patches.sh:843-881`
- **Problema:**
  - PATCH 20 cambiaba symlinks a OpenSSL 3.0
  - OpenSSL 3.0 shared libs enlazadas al sistema causan conflictos
- **Solución:**
  - Cambiado de OpenSSL 3.0 → OpenSSL 1.1
  - Symlinks ahora apuntan a `libssl.so.1.1` y `libcrypto.so.1.1`
  - Agregado comentario explicativo sobre conflictos con versión 3.0
- **Estado:** ⏳ PENDIENTE COMMIT

#### 3. Fix: PATCH 21 y 22 - Referencias a OpenSSL actualizadas
- **Commit:** Pendiente
- **Archivos:** `apply_compilation_patches.sh:883-989`
- **Cambio:** Actualizados mensajes de log para indicar OpenSSL 1.1 (estático)
- **Estado:** ⏳ PENDIENTE COMMIT

### 📋 Resumen Técnico del Fix:

**Problema raíz:** Enlazado mixto de bibliotecas estáticas y compartidas de OpenSSL

**Capas del fix:**
1. ✅ **PATCH 35**: FindCrypto.cmake prioriza `.a` sobre `.so`
2. ✅ **PATCH 20**: Symlinks apuntan a OpenSSL 1.1 (no 3.0)
3. ✅ **Symlinks manuales**: Ya corregidos a 1.1 en ambos directorios
4. ✅ **DataPipes reconstruido**: Con OpenSSL 1.1 estático

**Ventajas del fix:**
- Bibliotecas estáticas son autocontenidas (sin dependencias dinámicas)
- Evita conflictos con librerías del sistema
- Consistente entre DataPipes y Server
- Se aplica automáticamente en cada compilación

### ✅ Estado Final:
- PATCH 20, 21, 22 → Pusheados en commit `980269ed` "Docs: Update CHANGELOG - OpenSSL linking fix completed"
- PATCH 21 → Posteriormente corregido en sesión 2026-02-14 (lógica incorrecta)

---

## 🔄 SESIÓN: 2025-02-06 (Rama: claude/fix-install-script-ULBSP)

### ✅ Cambios Completados y Pusheados:

#### 1. Fix: Install Script - Rama Incorrecta
- **Commit:** `67fbc9c`
- **Archivo:** `install_teaspeak_complete.sh:77`
- **Problema:** El script apuntaba a rama temporal `claude/merge-to-main-01954FhCQayunKQ1gHcJhGhw`
- **Solución:** Cambiado a `REPO_BRANCH="main"`
- **Estado:** ✅ PUSHEADO

#### 2. Fix: LIBRARY_PATH Hardcodeado
- **Commit:** `aac9ac1`
- **Archivo:** `Server/Server/CMakeLists.txt:23-26`
- **Problema:** Ruta hardcodeada `/home/user/TeaSpeak/Server/Root/libraries/` causaba error `TomMath_INCLUDE_DIR not found`
- **Solución:** Implementada ruta relativa:
  ```cmake
  # Use relative path to libraries directory (works from any location)
  get_filename_component(LIBRARY_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../libraries/" ABSOLUTE)
  ```
- **Estado:** ✅ PUSHEADO

#### 3. Docs: Session Changelog
- **Commit:** `9d5afe5`
- **Archivo:** `CHANGELOG_SESIONES.md` (NUEVO)
- **Descripción:** Creado archivo de seguimiento de cambios entre sesiones
- **Estado:** ✅ PUSHEADO

#### 4. Fix: spin_lock → spin_mutex en NetTools.h
- **Commit:** `bf04ad3`
- **Archivo:** `Server/Server/file/local_server/NetTools.h:26,137`
- **Problema:** Uso de tipo `spin_lock` inexistente (el include era correcto `spin_mutex.h` pero el tipo usado era incorrecto)
- **Solución:** Cambiado tipo de variable:
  - Línea 26: `mutable spin_lock mutex{}` → `mutable spin_mutex mutex{}`
  - Línea 137: `spin_lock mutex{}` → `spin_mutex mutex{}`
- **Estado:** ✅ PUSHEADO

#### 5. Fix: Patch 33 en apply_compilation_patches.sh (CRÍTICO)
- **Commit:** `dd368f48` - "Fix: Correct Patch 33 and remaining spin_lock.h references"
- **Archivo:** `apply_compilation_patches.sh:1531-1550`
- **Estado:** ✅ PUSHEADO (resuelto en sesión siguiente)

#### 6. Fix: ServerCommandExecutor.h include incorrecto
- **Commit:** `7cf13e49` - "Fix: Replace remaining spin_lock.h includes with spin_mutex.h"
- **Archivo:** `Server/Server/server/src/client/shared/ServerCommandExecutor.h:3`
- **Estado:** ✅ PUSHEADO (resuelto en sesión siguiente)

### 📝 Contexto Importante:
- Usuario trabaja desde `/root/TeaSpeak` (symlink a `/home/user/TeaSpeak`)
- Usa `setup_teaspeak.sh` con autenticación GitHub
- Compilación CMake configuró OK, falló al compilar módulo `file`

---

## 🔄 SESIONES PREVIAS (Detectadas en git log)

### Commit: cd79679 - Fix spin_lock.h includes in file module
- **Fecha:** Reciente
- **Archivos:** Módulo file
- **Descripción:** Corrección de includes de spin_lock.h

### Commit: 56c7091 - Revert spin_lock back to spin_mutex
- **Descripción:** Reversión de cambio spin_lock → spin_mutex para compatibilidad 1.4.10

### Commit: d4a80e6 - Export uppercase BUILD_OS_TYPE and BUILD_OS_ARCH
- **Descripción:** Exportar variables en mayúsculas para CMake

### Commit: a8571be - Extend spin_mutex → spin_lock fix to server module
- **Descripción:** Aplicar fix de spin_mutex también al módulo server

### Commit: 92985cb - Add TeaSpeakLibrary include path
- **Archivo:** `server/CMakeLists.txt`
- **Descripción:** Agregar path de includes de TeaSpeakLibrary

---

## 🚧 TAREAS PENDIENTES (Actualizado 2026-02-14)

### ✅ Resuelto:
- [x] tearoot-server.cmake creado (cmake/config/)
- [x] Find*.cmake modules creados (TomMath, TomCrypt, StringVariable, DataPipes, CXXTerminal, Breakpad, Opus, Jemalloc, Crypto)
- [x] PATCH 21/22 corregidos (ya no causan exit 1)
- [x] TomMath regression (cache cleanup) resuelto
- [x] 4 library compilation failures resueltos (BoringSSL, zstd, breakpad, DataPipes)
- [x] Scripts build-helpers/libraries/ en git
- [x] download_libraries_custom.sh referenciado correctamente

### Prioridad ALTA:
- [ ] Ejecutar build completo para verificar que cmake configure correctamente
- [ ] Verificar que todos los find_package() resuelven correctamente

### Prioridad BAJA:
- [ ] Crear PR para mergear cambios a main

---

## 🔍 INFORMACIÓN DE DEBUGGING

### Estructura de Directorios Crítica:
```
/root/TeaSpeak/                           (symlink a /home/user/TeaSpeak)
├── Server/
│   ├── Root/
│   │   ├── TeaSpeak/                     (proyecto principal)
│   │   │   └── CMakeLists.txt           ← MODIFICADO (ruta relativa)
│   │   ├── libraries/                    (librerías compiladas)
│   │   │   ├── tommath/out/linux_amd64/
│   │   │   ├── tomcrypt/out/linux_amd64/
│   │   │   └── ...
│   │   └── build_teaspeak.sh
│   └── Server/                           (symlink a Root/TeaSpeak)
│       └── CMakeLists.txt               ← MISMO archivo que Root/TeaSpeak/
├── install_teaspeak_complete.sh         ← MODIFICADO (rama main)
└── setup_teaspeak.sh                     (script con GitHub auth)
```

### Rutas Importantes:
- **Librerías:** `/root/TeaSpeak/Server/Root/libraries/`
- **TomMath:** `/root/TeaSpeak/Server/Root/libraries/tommath/out/linux_amd64/`
- **CMakeLists principal:** `/root/TeaSpeak/Server/Server/CMakeLists.txt`

### Comandos del Usuario:
```bash
# Resetear a última versión
git reset --hard origin/claude/fix-install-script-ULBSP
git pull origin claude/fix-install-script-ULBSP

# Compilar
./setup_teaspeak.sh --github-user jorgebarreraa --github-token <TOKEN>
```

---

## 📌 NOTAS PARA PRÓXIMA SESIÓN

1. **LEER ESTE ARCHIVO PRIMERO** al iniciar nueva sesión
2. Verificar estado con: `git log --oneline -5`
3. Verificar rama actual: `git branch --show-current`
4. Si hay error de compilación, revisar sección "Información de Debugging"
5. Actualizar este archivo después de cada cambio importante

---

## 🛠️ TEMPLATE PARA NUEVAS SESIONES

```markdown
## 🔄 SESIÓN: [FECHA] (Rama: [NOMBRE_RAMA])

### ✅ Cambios Completados:

#### [NÚMERO]. [TÍTULO DEL CAMBIO]
- **Commit:** `[HASH]`
- **Archivo:** `[RUTA]:línea`
- **Problema:** [Descripción del problema]
- **Solución:** [Descripción de la solución]
- **Estado:** ✅ PUSHEADO / ⏳ PENDIENTE / ❌ FALLIDO

### 📝 Contexto:
[Información relevante de la sesión]

---
```

---

**Última actualización:** 2026-02-18
**Rama actual:** `claude/fix-install-script-ULBSP`
**Último commit:** (sesión 2026-02-18) Fix FindCXXTerminal.cmake: detect .so vs .a and create correct SHARED/STATIC IMPORTED target
