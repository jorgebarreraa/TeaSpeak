# 📦 Dependencias de TeaSpeak

Esta es la lista completa de todas las dependencias externas que utiliza TeaSpeak.

## 📊 Resumen

- **Total de dependencias externas:** 22 repositorios
- **Fuentes:**
  - GitHub: 12 repos
  - git.did.science: 7 repos
  - Google (chromium/boringssl): 3 repos

---

## 🔧 Dependencias del Sistema

Estas se instalan vía `apt-get`:

| Paquete | Descripción | Uso en TeaSpeak |
|---------|-------------|-----------------|
| `build-essential` | Herramientas de compilación básicas | Compilación C/C++ |
| `gcc` / `g++` | Compiladores C/C++ | Compilación de código |
| `cmake` | Sistema de build | Configuración de proyectos |
| `git` | Control de versiones | Clonar repositorios |
| `meson` | Build system | Compilación de librerías modernas |
| `ninja-build` | Build tool rápido | Backend de Meson |
| `autoconf` / `automake` | Auto-configuración | Builds antiguos |
| `libtool` | Librería de herramientas | Build de librerías |
| `libssl-dev` | OpenSSL headers | Criptografía |
| `libmysqlclient-dev` | MySQL client | Base de datos |
| `libcurl4-openssl-dev` | cURL con SSL | HTTP/HTTPS |
| `zlib1g-dev` | Compresión zlib | Compresión de datos |
| `python3` | Python 3 | Scripts de build |

**Rust Toolchain:**
- `cargo` - Package manager de Rust
- `rustc` - Compilador de Rust

*Necesarios para compilar el módulo RTC (WebRTC)*

---

## 📚 Dependencias de Código Externo

### 1. jsoncpp
- **URL original:** https://github.com/open-source-parsers/jsoncpp.git
- **Descripción:** Librería para parsear y generar JSON en C++
- **Licencia:** MIT / Public Domain
- **Uso:** Configuración del servidor, API

### 2. Thread-Pool
- **URL original:** https://git.did.science/WolverinDEV/ThreadPool.git
- **Descripción:** Pool de threads para C++
- **Autor:** WolverinDEV
- **Uso:** Manejo de concurrencia

### 3. tomcrypt / tommath
- **URLs:**
  - https://git.did.science/TeaSpeak/libraries/tomcrypt.git
  - https://git.did.science/TeaSpeak/libraries/tommath.git
- **Descripción:** Librerías criptográficas
- **Licencia:** Public Domain
- **Uso:** Criptografía de bajo nivel

### 4. CXXTerminal
- **URL original:** https://github.com/WolverinDEV/CXXTerminal.git
- **Descripción:** Terminal/Console library para C++
- **Autor:** WolverinDEV
- **Uso:** Interfaz de terminal del servidor

### 5. opus / opusfile
- **URLs:**
  - https://github.com/xiph/opus
  - https://github.com/xiph/opusfile.git
- **Descripción:** Codec de audio Opus
- **Licencia:** BSD
- **Uso:** Codec de voz principal

### 6. yaml-cpp
- **URL original:** https://github.com/jbeder/yaml-cpp.git
- **Descripción:** Parser YAML para C++
- **Licencia:** MIT
- **Uso:** Archivos de configuración

### 7. libevent
- **URL original:** https://github.com/libevent/libevent.git
- **Descripción:** Librería de eventos asíncronos
- **Licencia:** BSD
- **Uso:** Event loop, networking asíncrono

### 8. spdlog
- **URL original:** https://git.did.science/TeaSpeak/libraries/spdlog.git
- **Descripción:** Fast C++ logging library
- **Licencia:** MIT
- **Uso:** Sistema de logging

### 9. StringVariable
- **URL original:** https://github.com/WolverinDEV/StringVariable.git
- **Descripción:** String manipulation utilities
- **Autor:** WolverinDEV
- **Uso:** Manejo de strings

### 10. ed25519
- **URL original:** https://github.com/WolverinDEV/ed25519.git
- **Descripción:** Implementación de Ed25519 (firma digital)
- **Licencia:** Public Domain
- **Uso:** Licencias, autenticación

### 11. breakpad
- **URL original:** https://chromium.googlesource.com/breakpad/breakpad
- **Descripción:** Crash reporting library de Google
- **Licencia:** BSD
- **Uso:** Reportes de crashes

### 12. boringssl
- **URL original:** https://boringssl.googlesource.com/boringssl
- **Descripción:** Fork de OpenSSL de Google
- **Licencia:** OpenSSL/Apache 2.0
- **Uso:** Criptografía SSL/TLS

### 13. protobuf
- **URL original:** https://fuchsia.googlesource.com/third_party/protobuf
- **Versión:** v3.5.1.1
- **Descripción:** Protocol Buffers de Google
- **Licencia:** BSD
- **Uso:** Serialización de datos

### 14. DataPipes
- **URL original:** https://github.com/WolverinDEV/DataPipes.git
- **Descripción:** Data pipeline library
- **Autor:** WolverinDEV
- **Uso:** Procesamiento de streams de datos

### 15. jemalloc
- **URL original:** https://github.com/jemalloc/jemalloc.git
- **Rama:** dev
- **Descripción:** Memory allocator avanzado
- **Licencia:** BSD
- **Uso:** Gestión de memoria optimizada

### 16. libnice-prebuild
- **URL original:** https://git.did.science/TeaSpeak/libraries/libnice-prebuild.git
- **Descripción:** Librería ICE/STUN/TURN (precompilada)
- **Uso:** Negociación de NAT para WebRTC

### 17. glib2.0
- **URL original:** https://git.did.science/TeaSpeak/libraries/glib2.0.git
- **Descripción:** GLib library (requerida por libnice)
- **Uso:** Dependencia de libnice

### 18. openssl-prebuild
- **URL original:** https://git.did.science/TeaSpeak/libraries/openssl-prebuild.git
- **Descripción:** OpenSSL precompilado
- **Uso:** Criptografía SSL/TLS

### 19. zstd
- **URL original:** https://github.com/facebook/zstd.git
- **Descripción:** Fast compression algorithm de Facebook
- **Licencia:** BSD/GPLv2
- **Uso:** Compresión de datos

### 20. build-helpers
- **URL original:** https://github.com/WolverinDEV/build-helpers.git
- **Descripción:** Helper scripts para compilación
- **Autor:** WolverinDEV
- **Uso:** Scripts de build

---

## 🎯 Dependencias por Componente

### Servidor de Voz (VoiceServer)
- libevent (networking)
- opus (codec de voz)
- spdlog (logging)
- jemalloc (memory management)
- ed25519 (licencias)
- tomcrypt/tommath (crypto)
- jsoncpp (config/API)

### Módulo WebRTC
- libnice (ICE/STUN/TURN)
- glib2.0 (dependencia de libnice)
- boringssl (SSL/TLS)
- rust-webrtc (vía cargo)

### Sistema de Licencias
- ed25519 (firmas digitales)
- openssl (crypto)

### MusicBot
- opus (codec)
- FFmpeg (decodificación)
- yaml-cpp (config)

---

## 🔄 Migración a GitHub Personal

Para tener control total de estas dependencias, puedes migrarlas a tu cuenta de GitHub.

### ¿Por qué migrar?

1. **Permanencia:** Si git.did.science desaparece, tus builds seguirán funcionando
2. **Control:** Puedes hacer fixes y modificaciones
3. **Independencia:** No dependes de servidores externos
4. **Velocidad:** Clonar desde un solo servidor (GitHub)

### Cómo migrar

```bash
# 1. Configurar credenciales
export GITHUB_TOKEN="tu_token_de_github"
export GITHUB_USER="tu_usuario"

# 2. Ejecutar script de migración
./migrate_dependencies_to_github.sh

# 3. Actualizar .gitmodules
./update_gitmodules.sh

# 4. Compilar usando tus repos
./setup_teaspeak.sh --github-user tu_usuario
```

---

## 📝 Notas Importantes

### Repos que requieren atención especial:

1. **breakpad:** Necesita checkout de commit específico (f032e4c3) para C++17
2. **libevent:** Requiere parche para CMake 3.16
3. **protobuf:** Versión específica v3.5.1.1
4. **jemalloc:** Usa rama 'dev' en lugar de 'master'

### Problemas conocidos:

- **git.did.science:** Servidor puede ser lento o inaccesible
- **Google repos:** Requieren git 2.0+ para clonar correctamente
- **rust-webrtc:** Requiere parche en Cargo.toml (el script lo aplica automáticamente)

---

## 🆘 Solución de Problemas

### "Cannot clone from git.did.science"

**Solución:** Migra las dependencias a tu GitHub:
```bash
./migrate_dependencies_to_github.sh
```

### "Rust compilation fails"

**Solución:** El script aplica parches automáticamente. Si falla:
```bash
# Instalar Rust actualizado
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source $HOME/.cargo/env
```

### "CMake version too old"

**Solución:**
```bash
# Ubuntu 20.04+
sudo apt install cmake

# Ubuntu 18.04 o anterior
sudo snap install cmake --classic
```

---

## 📊 Estadísticas

- **Líneas de código total (estimado):** ~2.5 millones
- **Tamaño en disco (source):** ~1.2 GB
- **Tiempo de compilación (4 cores):** 30-45 minutos
- **Tiempo de compilación (16 cores):** 15-20 minutos

---

**Última actualización:** 2025-12-14
**Versión TeaSpeak:** 1.6.0
**Mantenedor:** Jorge Barrera (@jorgebarreraa)
