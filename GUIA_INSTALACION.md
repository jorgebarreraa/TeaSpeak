# 🚀 Guía Completa de Instalación de TeaSpeak

Esta guía te llevará paso a paso para instalar, configurar y compilar TeaSpeak Server con todas sus dependencias.

---

## 📋 Tabla de Contenidos

1. [Requisitos del Sistema](#-requisitos-del-sistema)
2. [Instalación Rápida](#-instalación-rápida)
3. [Instalación con Repositorios Personales](#-instalación-con-repositorios-personales)
4. [Configuración Avanzada](#-configuración-avanzada)
5. [Solución de Problemas](#-solución-de-problemas)
6. [Compilación Manual](#-compilación-manual)

---

## 💻 Requisitos del Sistema

### Sistema Operativo

- **Recomendado:** Ubuntu 20.04+ o Debian 10+
- **Probado en:**
  - Ubuntu 20.04 LTS ✅
  - Ubuntu 22.04 LTS ✅
  - Debian 11 ✅
  - Debian 12 ✅

### Hardware Mínimo

- **CPU:** 4 cores (recomendado 8+)
- **RAM:** 4 GB (recomendado 8 GB)
- **Disco:** 15 GB libres
- **Internet:** Conexión estable para descargar dependencias (~2 GB)

### Software Requerido

El script de instalación instalará automáticamente:
- GCC 9+ / G++
- CMake 3.16+
- Git 2.0+
- Rust (cargo, rustc)
- Meson + Ninja
- Librerías del sistema (OpenSSL, MySQL client, etc)

---

## ⚡ Instalación Rápida

Para instalar TeaSpeak con configuración por defecto (usa repositorios originales):

```bash
# 1. Clonar el repositorio
git clone https://github.com/jorgebarreraa/TeaSpeak.git
cd TeaSpeak

# 2. Ejecutar script de instalación
chmod +x setup_teaspeak.sh
./setup_teaspeak.sh

# 3. Esperar ~30-45 minutos (el script hace todo automáticamente)
```

### ¿Qué hace el script?

1. ✅ Verifica el sistema operativo
2. ✅ Instala todas las dependencias (gcc, cmake, rust, etc)
3. ✅ Descarga todas las librerías necesarias
4. ✅ Compila las librerías
5. ✅ Compila TeaSpeak Server
6. ✅ Verifica la compilación
7. ✅ Genera reportes y logs

Al finalizar, tendrás el servidor compilado en:
```
Server/Root/TeaSpeak/Server/server/out/linux_amd64/TeaSpeakServer
```

---

## 🔐 Instalación con Repositorios Personales

**⚠️ IMPORTANTE:** Como TeaSpeak es un proyecto antiguo sin actualizaciones, es muy recomendable migrar todas las dependencias a tu GitHub para evitar que los repositorios externos desaparezcan.

### Paso 1: Migrar Dependencias a tu GitHub

```bash
# 1. Obtener token de GitHub
# Ve a: https://github.com/settings/tokens
# Crea un token con permisos: repo (todos)

# 2. Configurar variables
export GITHUB_TOKEN="ghp_tu_token_aqui"
export GITHUB_USER="tu_usuario_github"

# 3. Ejecutar migración (tarda ~30-40 minutos)
chmod +x migrate_dependencies_to_github.sh
./migrate_dependencies_to_github.sh
```

Este script:
- Clona las 22 dependencias externas
- Crea repositorios en tu GitHub
- Sube todo el código (todas las ramas, tags, historial)
- Genera script de actualización

### Paso 2: Actualizar Referencias

```bash
# Ejecutar script generado
./update_gitmodules.sh

# Esto actualizará .gitmodules para usar tus repos
```

### Paso 3: Compilar con tus Repos

```bash
# Compilar usando tus repositorios
./setup_teaspeak.sh --github-user tu_usuario
```

### Ventajas de usar Repositorios Personales

✅ **Control Total:** Todas las dependencias bajo tu cuenta
✅ **Permanencia:** Si repos originales desaparecen, el tuyo sigue funcionando
✅ **Modificable:** Puedes hacer cambios y fixes personalizados
✅ **Independiente:** No dependes de git.did.science ni otros servidores
✅ **Velocidad:** Todo se descarga de GitHub (más rápido)

---

## 🛠️ Configuración Avanzada

### Opciones del Script de Instalación

```bash
./setup_teaspeak.sh [opciones]
```

**Opciones disponibles:**

| Opción | Descripción | Ejemplo |
|--------|-------------|---------|
| `--github-user <user>` | Usar forks personales | `--github-user jorgebarreraa` |
| `--build-type <type>` | Tipo de compilación | `--build-type stable` |
| `--skip-deps` | Saltar instalación de dependencias | `--skip-deps` |
| `--help` | Mostrar ayuda | `--help` |

**Tipos de Build:**

| Tipo | Descripción | Uso Recomendado |
|------|-------------|-----------------|
| `stable` | Build de producción estable | ✅ Producción |
| `optimized` | Optimizado (más rápido) | Desarrollo |
| `debug` | Con símbolos de debug | Debugging |
| `nightly` | Con optimizaciones + debug | Testing |

### Ejemplos de Uso

```bash
# Build estable con repos personales
./setup_teaspeak.sh --github-user jorgebarreraa --build-type stable

# Build optimizado, saltando deps (si ya están instaladas)
./setup_teaspeak.sh --skip-deps --build-type optimized

# Solo mostrar ayuda
./setup_teaspeak.sh --help
```

---

## 🔍 Verificación Post-Instalación

### Verificar el Binario

```bash
cd Server/Root/TeaSpeak/Server/server/out/linux_amd64

# Ver versión
./TeaSpeakServer --version

# Debería mostrar algo como:
# TeaSpeak-Server v1.6.0 [Build: xxxxxxxx]
```

### Verificar Librerías Compiladas

```bash
cd Server/Root/libraries

# Verificar que existen:
ls -lh jsoncpp/build
ls -lh opus/build
ls -lh event/build
ls -lh spdlog/build
```

### Ver Logs de Compilación

```bash
# Logs del script principal
cat /tmp/teaspeak_setup_*.log

# Logs de compilación de librerías
cat /tmp/teaspeak_setup_*.log.libraries

# Logs de compilación de TeaSpeak
cat /tmp/teaspeak_setup_*.log.teaspeak
```

---

## 🆘 Solución de Problemas

### Error: "CMake version too old"

**Problema:** CMake < 3.16

**Solución:**
```bash
# Ubuntu 20.04+
sudo apt update
sudo apt install cmake

# Ubuntu 18.04 o anterior
sudo snap install cmake --classic
```

### Error: "Cannot clone from git.did.science"

**Problema:** Servidor git.did.science inaccesible

**Solución:** Migra a GitHub personal
```bash
export GITHUB_TOKEN="tu_token"
export GITHUB_USER="tu_usuario"
./migrate_dependencies_to_github.sh
```

### Error: "Rust not found" o "cargo not found"

**Problema:** Rust no instalado

**Solución:**
```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source $HOME/.cargo/env
rustc --version  # verificar
```

### Error: "ld.gold causing issues"

**Problema:** Linker ld.gold causa problemas

**Solución:** El script lo deshabilita automáticamente, pero puedes hacerlo manualmente:
```bash
sudo mv /usr/bin/ld.gold /usr/bin/NOT_USED_ld.gold
```

### Error: Compilación falla con "undefined reference"

**Problema:** Librerías no compiladas con -fPIC

**Solución:**
```bash
cd Server/Root/libraries
export CXX_FLAGS="-fPIC"
export C_FLAGS="-fPIC"
bash build.sh
```

### Error: "No space left on device"

**Problema:** Disco lleno

**Solución:**
```bash
# Verificar espacio
df -h

# Limpiar archivos temporales
sudo apt clean
sudo apt autoclean
rm -rf /tmp/*

# Se necesitan al menos 15 GB libres
```

---

## 🔧 Compilación Manual

Si prefieres compilar manualmente en lugar de usar el script automático:

### 1. Instalar Dependencias

```bash
sudo apt update
sudo apt install -y \
    build-essential gcc g++ cmake git \
    libssl-dev libmysqlclient-dev \
    zlib1g-dev python3 meson ninja-build \
    autoconf automake libtool

# Instalar Rust
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source $HOME/.cargo/env
```

### 2. Clonar Repositorio

```bash
git clone https://github.com/jorgebarreraa/TeaSpeak.git
cd TeaSpeak
```

### 3. Descargar Librerías

```bash
cd Server/Root/libraries

# Limpiar enlaces simbólicos
rm -f tomcrypt tommath spdlog ed25519 openssl-prebuild libraries

# Descargar
bash download_libraries.sh

cd ../..
```

### 4. Compilar Librerías

```bash
cd Server/Root/libraries

export CXX_FLAGS="-fPIC"
export C_FLAGS="-fPIC"
export CMAKE_MAKE_OPTIONS="-j$(nproc)"

bash build.sh

cd ..
```

### 5. Compilar TeaSpeak

```bash
cd Server/Root

export build_os_type=linux
export build_os_arch=amd64

bash build_teaspeak.sh stable
```

### 6. Verificar

```bash
cd TeaSpeak/Server/server/out/linux_amd64
./TeaSpeakServer --version
```

---

## 📊 Tiempos de Compilación Estimados

| Hardware | Tiempo Total |
|----------|--------------|
| 4 cores, 8 GB RAM | 40-50 minutos |
| 8 cores, 16 GB RAM | 25-35 minutos |
| 16 cores, 32 GB RAM | 15-25 minutos |

### Desglose por Paso:

1. Instalación de dependencias: 5-10 min
2. Descarga de librerías: 5-10 min
3. Compilación de librerías: 15-25 min
4. Compilación de TeaSpeak: 10-20 min

---

## 🎯 Próximos Pasos

Una vez compilado exitosamente:

1. **Configurar el Servidor:**
   - Editar archivos de configuración
   - Configurar base de datos
   - Configurar licencia

2. **Ejecutar el Servidor:**
   ```bash
   cd Server/Root/TeaSpeak/Server/server/out/linux_amd64
   ./TeaSpeakServer
   ```

3. **Configurar como Servicio:**
   - Crear systemd service
   - Habilitar auto-inicio

4. **Backups y Mantenimiento:**
   - Configurar backups automáticos
   - Monitorear logs
   - Actualizaciones

---

## 📚 Recursos Adicionales

- **Dependencias:** Ver [DEPENDENCIES.md](DEPENDENCIES.md)
- **Arquitectura:** Ver README.md original
- **Troubleshooting:** Ver sección arriba

---

## 📞 Soporte

Si encuentras problemas:

1. **Revisar logs:**
   ```bash
   cat /tmp/teaspeak_setup_*.log
   ```

2. **Ejecutar diagnóstico:**
   ```bash
   ./diagnose_environment.sh
   ```

3. **Reportar issue:**
   - GitHub: https://github.com/jorgebarreraa/TeaSpeak/issues
   - Include logs y detalles del sistema

---

**Guía creada:** 2025-12-14
**Versión TeaSpeak:** 1.6.0
**Autor:** Jorge Barrera (@jorgebarreraa)
**Asistente:** Claude AI

---

## ⭐ Contribuciones

Si mejoras esta guía o encuentras errores, por favor:
1. Fork el repositorio
2. Haz tus cambios
3. Envía un Pull Request

¡Gracias por usar TeaSpeak! 🎉
