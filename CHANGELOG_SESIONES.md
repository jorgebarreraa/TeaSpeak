# 📋 Registro de Cambios por Sesión - TeaSpeak

Este archivo documenta TODOS los cambios realizados en cada sesión para facilitar la continuidad cuando se resetea el contexto.

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

**Última actualización:** 2026-02-14
**Rama actual:** `claude/fix-install-script-ULBSP`
**Último commit:** `eb6c87dd` + cambios de sesión 2026-02-14 (tearoot-server.cmake, Find*.cmake, PATCH 21/22)
**Rama actual:** `claude/fix-install-script-ULBSP`
**Último commit:** `aac9ac1` - Fix: Use relative path for LIBRARY_PATH in CMakeLists.txt
