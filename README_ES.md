# 🎙️ TeaSpeak Server - Instalación Automática

[![License](https://img.shields.io/badge/license-Custom-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)]()
[![Build](https://img.shields.io/badge/build-stable-green.svg)]()

Sistema completo de instalación automática para TeaSpeak Server con soporte para repositorios personales en GitHub.

---

## 🚀 Instalación Rápida (5 minutos de tu tiempo)

```bash
# 1. Clonar repositorio
git clone https://github.com/jorgebarreraa/TeaSpeak.git
cd TeaSpeak

# 2. Ejecutar instalación automática
chmod +x setup_teaspeak.sh
./setup_teaspeak.sh

# 3. Tomar un café ☕ (el script trabaja solo ~30-45 minutos)
```

**¡Listo!** El script hace todo automáticamente:
- ✅ Instala dependencias del sistema
- ✅ Descarga todas las librerías
- ✅ Compila todo
- ✅ Verifica la instalación

---

## 📋 ¿Qué hay en este Repositorio?

### 🛠️ Scripts Principales

| Script | Descripción | Uso |
|--------|-------------|-----|
| `setup_teaspeak.sh` | **Instalador maestro** - Todo en uno | ⭐ Recomendado para empezar |
| `migrate_dependencies_to_github.sh` | Migra dependencias a tu GitHub | Para tener control total |
| `diagnose_environment.sh` | Diagnóstico del sistema | Troubleshooting |

### 📚 Documentación

| Archivo | Contenido |
|---------|-----------|
| `GUIA_INSTALACION.md` | Guía completa paso a paso |
| `DEPENDENCIES.md` | Lista de todas las dependencias (22 repos) |
| `README_ES.md` | Este archivo |

---

## 🎯 Características Principales

### ✨ Instalación Automática

- ✅ **Detecta el sistema** automáticamente (Ubuntu, Debian)
- ✅ **Instala todas las dependencias** (GCC, CMake, Rust, librerías)
- ✅ **Descarga librerías** necesarias (22 repositorios)
- ✅ **Compila todo** con optimizaciones multi-core
- ✅ **Verifica** la compilación paso a paso
- ✅ **Logs detallados** para debugging

### 🔐 Soporte para GitHub Personal

¿Por qué migrar las dependencias a tu GitHub?

1. **Permanencia:** Si repositorios originales desaparecen, el tuyo sigue funcionando
2. **Control Total:** Puedes hacer modificaciones y fixes
3. **Independencia:** No dependes de `git.did.science` ni otros servidores
4. **Velocidad:** Todo desde GitHub (más rápido)

```bash
# Migrar todas las dependencias (22 repos) a tu GitHub
export GITHUB_TOKEN="tu_token"
export GITHUB_USER="tu_usuario"
./migrate_dependencies_to_github.sh

# Luego compilar usando tus repos
./setup_teaspeak.sh --github-user tu_usuario
```

### 🚀 Compilación Optimizada

El script usa todos los núcleos de tu CPU automáticamente:

- 4 cores: ~40 minutos
- 8 cores: ~25 minutos
- 16 cores: ~15 minutos

---

## 📖 Uso Detallado

### Opción 1: Instalación Estándar (Repos Originales)

```bash
./setup_teaspeak.sh
```

Usa los repositorios originales de:
- GitHub (jsoncpp, opus, etc)
- git.did.science (TeaSpeak libs)
- Google (breakpad, boringssl)

### Opción 2: Instalación con Repos Personales (Recomendado)

**Paso 1:** Migrar dependencias

```bash
# Obtener token: https://github.com/settings/tokens
# Permisos necesarios: repo (todos)

export GITHUB_TOKEN="ghp_xxxxxxxxxxxxx"
export GITHUB_USER="jorgebarreraa"  # tu usuario

./migrate_dependencies_to_github.sh
```

Esto:
- Clona las 22 dependencias
- Crea repos en tu GitHub
- Sube todo el código
- Genera script de actualización

**Paso 2:** Actualizar referencias

```bash
./update_gitmodules.sh
```

**Paso 3:** Compilar

```bash
./setup_teaspeak.sh --github-user jorgebarreraa
```

### Opciones Avanzadas

```bash
# Ver todas las opciones
./setup_teaspeak.sh --help

# Build optimizado
./setup_teaspeak.sh --build-type optimized

# Saltar instalación de deps (si ya las tienes)
./setup_teaspeak.sh --skip-deps

# Build debug
./setup_teaspeak.sh --build-type debug

# Combinaciones
./setup_teaspeak.sh --github-user jorgebarreraa --build-type stable
```

---

## 🎓 Tipos de Build

| Tipo | Optimización | Debug Info | Uso Recomendado |
|------|--------------|------------|-----------------|
| `stable` | ✅ Alta | ❌ No | ⭐ Producción |
| `optimized` | ✅✅ Máxima | ⚠️ Mínima | Desarrollo |
| `debug` | ❌ No | ✅✅ Completa | Debugging |
| `nightly` | ✅ Alta | ✅ Completa | Testing |

---

## 📂 Estructura del Proyecto

```
TeaSpeak/
├── setup_teaspeak.sh                      # ⭐ Instalador principal
├── migrate_dependencies_to_github.sh      # Migrador de repos
├── diagnose_environment.sh                # Diagnóstico
│
├── Server/
│   ├── Root/
│   │   ├── build_teaspeak.sh             # Build de TeaSpeak
│   │   ├── setup_environment.sh          # Setup de entorno
│   │   ├── libraries/
│   │   │   ├── download_libraries.sh     # Descarga libs
│   │   │   └── build.sh                  # Compila libs
│   │   └── TeaSpeak/                     # Código fuente
│   │       └── Server/server/
│   │           └── out/linux_amd64/
│   │               └── TeaSpeakServer    # ⭐ Binario final
│   │
│   └── rtc/                              # Módulo WebRTC
│
├── GUIA_INSTALACION.md                   # Guía completa
├── DEPENDENCIES.md                       # Lista de dependencias
└── README_ES.md                          # Este archivo
```

---

## 🔍 Verificación

### Después de compilar:

```bash
# Ver versión del servidor
cd Server/Root/TeaSpeak/Server/server/out/linux_amd64
./TeaSpeakServer --version

# Debería mostrar:
# TeaSpeak-Server v1.6.0 [Build: xxxxxxxx]
```

### Ver logs:

```bash
# Log principal
cat /tmp/teaspeak_setup_*.log

# Log de librerías
cat /tmp/teaspeak_setup_*.log.libraries

# Log de TeaSpeak
cat /tmp/teaspeak_setup_*.log.teaspeak
```

---

## 🆘 Solución de Problemas

### Script de Diagnóstico

```bash
./diagnose_environment.sh
```

Este script verifica:
- Sistema operativo
- Herramientas instaladas (gcc, cmake, rust)
- Versiones correctas
- Espacio en disco
- Configuración del linker

### Problemas Comunes

| Problema | Solución |
|----------|----------|
| "CMake too old" | `sudo snap install cmake --classic` |
| "Rust not found" | Ver sección de Rust abajo |
| "feature not found" (Rust) | **Usas stable, necesitas NIGHTLY** → `rustup default nightly` |
| "Cannot clone git.did.science" | Migra a GitHub personal |
| "ld.gold issues" | El script lo deshabilita automáticamente |
| "No space left" | Necesitas 15 GB libres |

### Instalar Rust Manualmente (NIGHTLY REQUERIDO)

⚠️ **IMPORTANTE:** TeaSpeak requiere **Rust NIGHTLY** (no stable)

```bash
# Instalar rustup
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source $HOME/.cargo/env

# CRÍTICO: Instalar y usar nightly
rustup install nightly
rustup default nightly

# Verificar que es nightly
rustc --version  # Debe mostrar "nightly"
```

**¿Por qué nightly?** El código usa features inestables como `backtrace`, `core_intrinsics`, `drain_filter`, etc.

---

## 💻 Requisitos del Sistema

### Mínimos

- Ubuntu 18.04+ / Debian 10+
- 4 CPU cores
- 4 GB RAM
- 15 GB disco
- Conexión a Internet

### Recomendados

- Ubuntu 22.04 LTS
- 8 CPU cores
- 8 GB RAM
- 20 GB SSD
- Conexión rápida

---

## 📊 Dependencias

**Total: 22 repositorios externos**

Ver lista completa en: [DEPENDENCIES.md](DEPENDENCIES.md)

### Por Fuente:

- **GitHub:** 12 repos
  - jsoncpp, opus, yaml-cpp, libevent, ed25519, StringVariable, CXXTerminal, DataPipes, jemalloc, zstd, build-helpers, opusfile

- **git.did.science:** 7 repos
  - Thread-Pool, tomcrypt, tommath, spdlog, libnice-prebuild, glib2.0, openssl-prebuild

- **Google:** 3 repos
  - breakpad, boringssl, protobuf

### Principales:

| Librería | Uso |
|----------|-----|
| **opus** | Codec de voz |
| **libevent** | Event loop / networking |
| **spdlog** | Logging system |
| **jsoncpp** | JSON parsing |
| **ed25519** | Firmas digitales (licencias) |
| **rust-webrtc** | WebRTC support |

---

## 🔄 Recompilar

```bash
cd Server/Root

export build_os_type=linux
export build_os_arch=amd64

# Recompilar solo TeaSpeak (las libs ya están compiladas)
bash build_teaspeak.sh stable

# Recompilar todo (librerías + TeaSpeak)
cd libraries && bash build.sh && cd ..
bash build_teaspeak.sh stable
```

---

## 🚀 Ejecutar el Servidor

```bash
cd Server/Root/TeaSpeak/Server/server/out/linux_amd64
./TeaSpeakServer

# Con configuración personalizada
./TeaSpeakServer --config /path/to/config.yml
```

---

## 📝 Notas Importantes

### ⚠️ Proyecto Antiguo

TeaSpeak **NO recibe actualizaciones** desde hace tiempo. Por eso es **MUY RECOMENDABLE** migrar todas las dependencias a tu GitHub:

```bash
./migrate_dependencies_to_github.sh
```

Así garantizas que:
- ✅ Tu proyecto seguirá compilando aunque repos originales desaparezcan
- ✅ Tienes control total de las dependencias
- ✅ Puedes hacer modificaciones si necesitas

### 🔐 Token de GitHub

Para migrar dependencias necesitas un token:

1. Ve a: https://github.com/settings/tokens
2. "Generate new token (classic)"
3. Permisos: `repo` (todos)
4. Copia el token
5. `export GITHUB_TOKEN="tu_token"`

**No compartas tu token con nadie.**

---

## 📞 Soporte

### Reportar Problemas

1. Ejecuta diagnóstico:
   ```bash
   ./diagnose_environment.sh > diagnostic.log
   ```

2. Crea un issue en GitHub:
   - Incluye `diagnostic.log`
   - Incluye logs de compilación
   - Describe el error

### Recursos

- **Guía de Instalación:** [GUIA_INSTALACION.md](GUIA_INSTALACION.md)
- **Dependencias:** [DEPENDENCIES.md](DEPENDENCIES.md)
- **Issues:** https://github.com/jorgebarreraa/TeaSpeak/issues

---

## 🤝 Contribuir

¿Mejoraste algo? ¡Compártelo!

1. Fork el repositorio
2. Crea una rama: `git checkout -b feature/mejora`
3. Commit: `git commit -m "Descripción"`
4. Push: `git push origin feature/mejora`
5. Abre un Pull Request

---

## 📜 Licencia

Ver archivo `LICENSE` para detalles.

---

## 🙏 Créditos

- **TeaSpeak Original:** WolverinDEV
- **Fork y Scripts:** Jorge Barrera (@jorgebarreraa)
- **Asistente de Desarrollo:** Claude AI (Anthropic)

### Librerías de Terceros

Ver [DEPENDENCIES.md](DEPENDENCIES.md) para lista completa de licencias.

---

## ⭐ Si te sirvió este proyecto

- Dale una ⭐ en GitHub
- Compártelo con otros
- Reporta bugs
- Contribuye mejoras

---

## 📈 Estado del Proyecto

- ✅ Scripts de instalación automática
- ✅ Migración a GitHub personal
- ✅ Documentación completa en español
- ✅ Soporte para Ubuntu 20.04+
- ✅ Compilación optimizada multi-core
- ⏳ Configuración como servicio systemd (próximamente)
- ⏳ Docker support (próximamente)

---

**Última actualización:** 2025-12-14
**Versión TeaSpeak:** 1.6.0
**Versión Scripts:** 2.0

---

## 🎉 ¡Disfruta TeaSpeak!

Si llegaste hasta aquí, ya sabes todo lo necesario para instalar y compilar TeaSpeak.

**¿Listo para empezar?**

```bash
./setup_teaspeak.sh
```

¡Buena suerte! 🚀
