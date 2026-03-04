# 🔀 Guía para Hacer Fork de TODAS las Dependencias

**Objetivo:** Tener control total de todas las dependencias en tu cuenta GitHub con parches aplicados permanentemente.

---

## 📋 Lista Completa de Repositorios a Forkear (24 total)

### ✅ Paso 1: Hacer Fork Manualmente en GitHub

Visita cada URL y haz clic en "Fork":

#### GitHub.com (12 repos):

| # | Repo Original | URL para Fork | Parche Necesario |
|---|---------------|---------------|------------------|
| 1 | `open-source-parsers/jsoncpp` | https://github.com/open-source-parsers/jsoncpp/fork | ❌ No |
| 2 | `WolverinDEV/CXXTerminal` | https://github.com/WolverinDEV/CXXTerminal/fork | ❌ No |
| 3 | `WolverinDEV/StringVariable` | https://github.com/WolverinDEV/StringVariable/fork | ❌ No |
| 4 | `WolverinDEV/ed25519` | https://github.com/WolverinDEV/ed25519/fork | ❌ No |
| 5 | `WolverinDEV/DataPipes` | https://github.com/WolverinDEV/DataPipes/fork | ❌ No |
| 6 | `WolverinDEV/build-helpers` | https://github.com/WolverinDEV/build-helpers/fork | ✅ **SÍ - Breakpad C++17** |
| 7 | `WolverinDEV/ThreadPool` | **No disponible** | Ver git.did.science |
| 8 | `xiph/opus` | https://github.com/xiph/opus/fork | ❌ No |
| 9 | `xiph/opusfile` | https://github.com/xiph/opusfile/fork | ❌ No |
| 10 | `jbeder/yaml-cpp` | https://github.com/jbeder/yaml-cpp/fork | ❌ No |
| 11 | `libevent/libevent` | https://github.com/libevent/libevent/fork | ✅ **SÍ - CMake 3.16** |
| 12 | `jemalloc/jemalloc` | https://github.com/jemalloc/jemalloc/fork | ❌ No (branch: dev) |
| 13 | `facebook/zstd` | https://github.com/facebook/zstd/fork | ❌ No |

#### Repos en git.did.science (6 repos) - ⚠️ NO SE PUEDEN FORKEAR DIRECTAMENTE

Estos repos están en `git.did.science` que puede desaparecer. **Necesitas clonarlos y subirlos a tu GitHub:**

| # | Repo Original | Clonar a tu GitHub como | Parche |
|---|---------------|-------------------------|--------|
| 14 | `git.did.science/WolverinDEV/ThreadPool` | `Thread-Pool` | ❌ No |
| 15 | `git.did.science/TeaSpeak/libraries/tomcrypt` | `tomcrypt` | ❌ No |
| 16 | `git.did.science/TeaSpeak/libraries/tommath` | `tommath` | ❌ No |
| 17 | `git.did.science/TeaSpeak/libraries/spdlog` | `spdlog` | ❌ No |
| 18 | `git.did.science/TeaSpeak/libraries/libnice-prebuild` | `libnice-prebuild` | ❌ No |
| 19 | `git.did.science/TeaSpeak/libraries/glib2.0` | `glib2.0` | ❌ No |
| 20 | `git.did.science/TeaSpeak/libraries/openssl-prebuild` | `openssl-prebuild` | ❌ No |

#### Google Repos (3 repos) - ⚠️ NO SE PUEDEN FORKEAR

**Estos son de Google y necesitas clonarlos manualmente:**

| # | Repo Original | Clonar a tu GitHub como | Parche |
|---|---------------|-------------------------|--------|
| 21 | `chromium.googlesource.com/breakpad/breakpad` | `breakpad` | ✅ **SÍ - Ver abajo** |
| 22 | `boringssl.googlesource.com/boringssl` | `boringssl` | ❌ No |
| 23 | `fuchsia.googlesource.com/third_party/protobuf` | `protobuf` | ❌ No (tag: v3.5.1.1) |

#### Repos Rust - WolverinDEV (2 repos):

| # | Repo Original | URL para Fork | Parche |
|---|---------------|---------------|--------|
| 24 | `WolverinDEV/rust-webrtc` | https://github.com/WolverinDEV/rust-webrtc/fork | ✅ **SÍ - Cargo.toml slog** |
| 25 | `WolverinDEV/rust-libnice` | https://github.com/WolverinDEV/rust-libnice/fork | ❌ No |

---

## 🔧 Paso 2: Aplicar Parches a tus Forks

### Parche 1: `build-helpers` - Breakpad C++17

**Archivo:** `libraries/build_breakpad.sh` (línea 38)

**Cambiar:**
```bash
make CXXFLAGS="-std=c++11 ${CXX_FLAGS} -static-libgcc -static-libstdc++" CFLAGS="${C_FLAGS}" ${MAKE_OPTIONS}
```

**Por:**
```bash
# CRITICAL: Pass C++17 to configure so Makefile is generated with correct flags
# Breakpad requires C++14+ for std::make_unique and std::string_view
CXXFLAGS="-std=c++17 ${CXX_FLAGS} -static-libgcc -static-libstdc++" \
CFLAGS="${C_FLAGS}" \
../../configure --prefix=`pwd` || { echo "ERROR: Breakpad configure failed!"; exit 1; }

# Build with the flags from configure
make ${MAKE_OPTIONS} || { echo "ERROR: Breakpad compilation failed with C++17!"; exit 1; }

make install || { echo "ERROR: Breakpad install failed!"; exit 1; }
```

---

### Parche 2: `libevent` - CMake 3.16 Compatibility

**Archivo:** `cmake/AddLinkerFlags.cmake`

Ya está incluido en `download_libraries.sh` (líneas 36-60), pero aplícalo permanentemente en tu fork.

---

### Parche 3: `breakpad` - Múltiples Fixes

**a) Checkout commit específico:**
```bash
git checkout f032e4c3
```

**b) Archivo:** `src/processor/minidump.cc` (línea 2098)

**Cambiar:**
```cpp
numeric_limits<off_t>::max()
```

**Por:**
```cpp
static_cast<uint64_t>(numeric_limits<off_t>::max())
```

**c) Archivo:** `src/common/linux/dump_symbols.cc`

Agregar después de los includes:
```cpp
// Define missing ELF constants for older systems
#ifndef SHF_COMPRESSED
#define SHF_COMPRESSED (1 << 11)
#endif

#ifndef ELFCOMPRESS_ZLIB
#define ELFCOMPRESS_ZLIB 1
#endif

#ifndef ELFCOMPRESS_ZSTD
#define ELFCOMPRESS_ZSTD 2
#endif

#ifndef EM_RISCV
#define EM_RISCV 243
#endif
```

---

### Parche 4: `rust-webrtc` - Fix slog dependency

**Archivo:** `Cargo.toml`

**Buscar:**
```toml
slog = {}
```
o
```toml
slog = { features = [...] }
```

**Cambiar por:**
```toml
slog = { version = "2.7", features = [...] }
```

---

## 🚀 Paso 3: Usar tus Forks

Una vez que todos los repos estén en tu cuenta con parches aplicados:

```bash
./setup_teaspeak.sh --github-user TU_USUARIO --build-type stable
```

Esto usará **automáticamente** TODOS tus forks en lugar de los repos originales.

---

## ✅ Ventajas de Esta Estrategia

1. ✅ **Control Total** - Eres dueño de todas las dependencias
2. ✅ **Parches Permanentes** - No necesitas scripts de parche
3. ✅ **Inmune a link rot** - Si repos externos desaparecen, tienes copias
4. ✅ **Reproducible** - Funciona en cualquier VPS
5. ✅ **Doblemente robusto:**
   - Con `--github-user`: Usa tus repos parchados
   - Sin `--github-user`: Scripts automáticos aplican parches

---

## 📝 Notas Importantes

- Los repos de `git.did.science` pueden desaparecer en cualquier momento
- Los repos de Google NO permiten fork - debes clonar y subir manualmente
- Mantén los nombres de repo EXACTOS para que funcione con `--github-user`

