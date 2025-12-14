# 🚀 Instalación Automática de TeaSpeak Server

## ✅ Instalación en UN SOLO PASO

Para instalar y compilar TeaSpeak Server **automáticamente con TODAS las dependencias**:

```bash
cd ~/TeaSpeak
./setup_teaspeak.sh --github-user jorgebarreraa --build-type stable
```

¡Eso es TODO! El script hace:

1. ✅ Instala todas las dependencias del sistema (gcc, cmake, rust nightly, etc.)
2. ✅ Descarga TODAS las 24 librerías desde **tus repos en GitHub**
3. ✅ Aplica parches automáticamente (breakpad C++17, Cargo.toml, etc.)
4. ✅ Modifica configuraciones para usar TUS repositorios
5. ✅ Compila todas las librerías
6. ✅ Compila TeaSpeak Server completo
7. ✅ Habilita TODOS los componentes (Jemalloc, Web Client, MusicBot, etc.)

---

## 📊 Tipos de Build Disponibles

```bash
# Build estable (recomendado para producción)
./setup_teaspeak.sh --github-user jorgebarreraa --build-type stable

# Build optimizado (máximo rendimiento)
./setup_teaspeak.sh --github-user jorgebarreraa --build-type optimized

# Build debug (para desarrollo)
./setup_teaspeak.sh --github-user jorgebarreraa --build-type debug
```

---

## 🔧 Parches Aplicados AUTOMÁTICAMENTE

El script aplica estos parches sin intervención manual:

### 1. **Breakpad C++17** (apply_build_fixes.sh)
- **Problema:** Breakpad usa C++11 por defecto y falla con `std::make_unique`
- **Solución:** Parchea `build-helpers/libraries/build_breakpad.sh` para usar C++17
- **Cuándo:** Después de descargar librerías, antes de compilar

### 2. **Cargo.toml - Repos del Usuario** (update_rust_cargo_dependencies)
- **Problema:** `Server/rtc/Cargo.toml` tiene URLs hardcoded a repos de WolverinDEV
- **Solución:** Reemplaza URLs con `github.com/jorgebarreraa/*`
- **Cuándo:** Después de descargar librerías, antes de compilar

### 3. **Cargo Cache Limpieza** (apply_build_fixes.sh)
- **Problema:** Cargo puede tener checkouts corruptos de rust-webrtc
- **Solución:** Limpia `.cargo/git/checkouts` y `.cargo/git/db` antes de compilar
- **Cuándo:** Al inicio de apply_build_fixes.sh

---

## 📦 Tus 24 Repositorios Migrados

Cuando usas `--github-user jorgebarreraa`, el script usa **TUS repos**:

### Sección 1: git.did.science (7)
- Thread-Pool, tomcrypt, tommath, spdlog
- libnice-prebuild, glib2.0, openssl-prebuild

### Sección 2: Google (3)
- breakpad, boringssl, protobuf

### Sección 3: GitHub - WolverinDEV (7)
- **build-helpers** (CRÍTICO - tiene build_breakpad.sh)
- CXXTerminal, StringVariable, ed25519, DataPipes
- **rust-webrtc** (CRÍTICO - Cargo.toml con slog)
- rust-libnice

### Sección 4: GitHub - Otros (7)
- jsoncpp, opus, opusfile, yaml-cpp
- libevent, jemalloc, zstd

---

## ⚙️ Componentes HABILITADOS

El script habilita TODOS los componentes (nada deshabilitado):

- ✅ **Jemalloc** - Allocador de memoria avanzado
- ✅ **Web Client** - Cliente web integrado
- ✅ **MusicBot** - Bot de música con FFmpeg y YouTube
- ✅ **WebRTC** - Soporte de WebRTC con libnice
- ✅ **FileServer** - Servidor de archivos
- ✅ **QueryServer** - API de consultas
- ✅ **BoringSSL** - Criptografía optimizada

---

## 🐛 Solución de Problemas

### Error: "Breakpad compilation failed"
**Causa:** El parche de C++17 no se aplicó correctamente

**Solución:**
```bash
# Limpiar y reinstalar
rm -rf Server/Root/libraries/breakpad
./setup_teaspeak.sh --github-user jorgebarreraa --build-type stable
```

### Error: "dependency (slog) specified without providing..."
**Causa:** Cargo está usando un checkout corrupto de rust-webrtc

**Solución:** El script automáticamente limpia el cache. Si persiste:
```bash
# Limpiar cache manualmente
rm -rf ~/.cargo/git/checkouts/rust-webrtc-*
rm -rf ~/.cargo/git/db/rust-webrtc-*
./setup_teaspeak.sh --github-user jorgebarreraa --build-type stable
```

### Error: "El script deberia detenerse al tener algun error"
**Solución:** Ya está arreglado. Todos los scripts tienen `set -e` ahora.

---

## 📋 Verificar Instalación Exitosa

Después de la compilación exitosa, verifica:

```bash
# Verificar binarios compilados
ls -lh Server/cmake-build-*/bin/

# Deberías ver:
# - TeaSpeakServer
# - teastart_minimal
# - libteaspeak_music_bot_*.so
```

---

## 🎯 Ejecutar TeaSpeak Server

```bash
cd Server/cmake-build-stable/bin/
./TeaSpeakServer
```

---

## 📚 Documentación Adicional

- **CAMBIOS_REALIZADOS.md** - Lista completa de todos los cambios y fixes
- **FORK_ALL_DEPENDENCIES.md** - Guía de migración de los 24 repos

---

## ✅ Ventajas de Usar TUS Repositorios

1. **Control Total** - Puedes modificar cualquier dependencia
2. **Inmunidad a "Link Rot"** - Si repos originales desaparecen, los tuyos siguen ahí
3. **Parches Permanentes** - Puedes aplicar fixes directamente en tus repos
4. **Sin Dependencias Upstream** - No afectado por cambios de los autores originales
5. **Historial Completo** - Tienes todo el historial de git

---

**¿Preguntas?** Revisa `CAMBIOS_REALIZADOS.md` para detalles técnicos completos.
