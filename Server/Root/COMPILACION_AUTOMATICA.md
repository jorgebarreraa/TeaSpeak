# TeaSpeak - Compilación Automatizada

Este documento explica cómo compilar TeaSpeak de forma completamente automatizada usando los nuevos scripts.

## Inicio Rápido

### Opción 1: Compilación en un solo comando

```bash
./compile_teaspeak_auto.sh
```

Este script hace **TODO** automáticamente:
- ✓ Configura las variables de entorno necesarias
- ✓ Maneja el problema del linker ld.gold
- ✓ Descarga todas las bibliotecas requeridas
- ✓ Verifica las herramientas necesarias
- ✓ Compila rtclib (glib y dependencias)
- ✓ Compila el servidor TeaSpeak

### Opción 2: Verificar el entorno primero

Si quieres verificar que tienes todas las dependencias instaladas antes de compilar:

```bash
./setup_environment.sh
```

Este script verifica:
- ✓ Herramientas requeridas (gcc, cmake, rust, etc.)
- ✓ Versiones correctas
- ✓ Configuración del linker
- ✓ Espacio en disco
- ✓ Y proporciona instrucciones de instalación si falta algo

Luego compila:

```bash
./compile_teaspeak_auto.sh
```

## Tipos de Compilación

Puedes especificar el tipo de compilación como argumento:

```bash
./compile_teaspeak_auto.sh debug      # Compilación de depuración
./compile_teaspeak_auto.sh nightly    # Compilación nocturna
./compile_teaspeak_auto.sh optimized  # Compilación optimizada (por defecto)
./compile_teaspeak_auto.sh stable     # Compilación estable
```

## Qué Problemas Resuelven Estos Scripts

### 1. Problema del Linker ld.gold

**Problema original:**
```
Error: unsupported reloc 42 against global symbol
```

**Solución automática:**
El script `compile_teaspeak_auto.sh` detecta y deshabilita automáticamente ld.gold si está presente:

```bash
sudo mv /usr/bin/ld.gold /usr/bin/NOT_USED_ld.gold
```

### 2. Variables de Entorno

**Antes (manual):**
```bash
export build_os_type=linux
export build_os_arch=amd64
export crypto_library_path="/ruta/completa/a/openssl-prebuild/linux_amd64"
export CMAKE_MAKE_OPTIONS="-j$(nproc --all)"
# ... y más configuraciones
```

**Ahora (automático):**
Todo esto se configura automáticamente dentro del script.

### 3. Orden de Compilación

El script asegura el orden correcto:
1. Descarga de bibliotecas
2. Compilación de rtclib (con glib)
3. Compilación de TeaSpeak

### 4. Detección de Errores

El script se detiene inmediatamente si algo falla (`set -e`) y muestra mensajes claros de error.

## Estructura de los Scripts

```
Server/Root/
├── compile_teaspeak_auto.sh      # Script principal de compilación
├── setup_environment.sh          # Verificación de requisitos
├── libraries/
│   └── download_libraries.sh     # Descarga de bibliotecas (llamado automáticamente)
├── build_teaspeak.sh             # Script original de compilación (llamado automáticamente)
└── TeaSpeak/
    └── rtclib/
        ├── build_glib.sh         # Compilación de glib (llamado automáticamente)
        └── generate_shared_library.sh  # Compilación de rtclib (llamado automáticamente)
```

## Requisitos del Sistema

### Sistema Operativo
- Ubuntu 14.04 o superior
- Debian-based systems

### Herramientas Requeridas

El script `setup_environment.sh` verifica todas estas:

- **Compiladores:**
  - gcc >= 9.x
  - g++ >= 9.x

- **Build Tools:**
  - cmake >= 3.16
  - make
  - autoconf
  - pkg-config

- **Rust Toolchain:**
  - cargo
  - rustc

- **Build Systems:**
  - meson
  - ninja

- **Utilities:**
  - git
  - wget
  - tar

### Instalación de Dependencias

#### Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake git wget autoconf \
    software-properties-common pkg-config

# GCC 9 (si no está instalado)
sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y
sudo apt-get update
sudo apt-get install gcc-9 g++-9 -y
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 60 \
    --slave /usr/bin/g++ g++ /usr/bin/g++-9
```

#### Rust:
```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source $HOME/.cargo/env
```

#### Meson y Ninja:
```bash
pip3 install meson ninja
```

## Flujo de Trabajo Recomendado

### Primera vez:

```bash
# 1. Verificar el entorno
./setup_environment.sh

# 2. Compilar (tipo optimized por defecto)
./compile_teaspeak_auto.sh

# 3. Los binarios estarán en:
cd TeaSpeak/build/
```

### Recompilaciones:

```bash
# Simplemente ejecuta el script de nuevo
./compile_teaspeak_auto.sh optimized
```

### Limpiar y recompilar desde cero:

```bash
# Limpiar el directorio de compilación
rm -rf TeaSpeak/build/

# Opcional: limpiar rtclib también
rm -f TeaSpeak/rtclib/libteaspeak_rtc.so

# Recompilar
./compile_teaspeak_auto.sh
```

## Solución de Problemas

### "Missing required tools"

Ejecuta `setup_environment.sh` para ver qué falta e instálalo según las instrucciones.

### "Failed to build rtclib"

Verifica que:
- Rust está instalado correctamente: `rustc --version`
- Meson está instalado: `meson --version`
- Las bibliotecas de OpenSSL están en: `libraries/openssl-prebuild/linux_amd64/`

### "unsupported reloc 42 against global symbol"

Este error debería ser manejado automáticamente por el script. Si persiste:

```bash
sudo mv /usr/bin/ld.gold /usr/bin/NOT_USED_ld.gold
```

### Espacio en disco insuficiente

La compilación puede requerir 10GB o más. Verifica:

```bash
df -h .
```

## Variables de Entorno (Avanzado)

Si quieres personalizar la compilación, puedes establecer estas variables antes de ejecutar el script:

```bash
# Tipo de OS (por defecto: linux)
export build_os_type=linux

# Arquitectura (por defecto: amd64)
export build_os_arch=amd64

# Número de trabajos paralelos (por defecto: número de CPUs)
export CMAKE_MAKE_OPTIONS="-j8"

# Luego compila
./compile_teaspeak_auto.sh
```

## Comparación: Antes vs Ahora

### Antes (Manual):
```bash
# 1. Configurar variables de entorno
export build_os_type=linux
export build_os_arch=amd64
export crypto_library_path="$(pwd)/libraries/openssl-prebuild/linux_amd64"

# 2. Manejar ld.gold manualmente
sudo mv /usr/bin/ld.gold /usr/bin/NOT_USED_ld.gold

# 3. Descargar bibliotecas
cd libraries
./download_libraries.sh
cd ..

# 4. Compilar rtclib
cd TeaSpeak/rtclib
crypto_library_path="$crypto_library_path" ./generate_shared_library.sh
cd ../..

# 5. Compilar TeaSpeak
./build_teaspeak.sh optimized
```

### Ahora (Automatizado):
```bash
./compile_teaspeak_auto.sh
```

## Logs y Debugging

El script proporciona salida colorizada:
- 🟢 **[INFO]** - Operaciones normales
- 🟡 **[WARN]** - Advertencias
- 🔴 **[ERROR]** - Errores

Para debugging más detallado, puedes ejecutar con bash -x:

```bash
bash -x ./compile_teaspeak_auto.sh
```

## Contribuciones

Estos scripts fueron creados para simplificar el proceso de compilación de TeaSpeak.
Si encuentras problemas o tienes sugerencias, por favor repórtalos.

---

**Nota:** Estos scripts mantienen compatibilidad con los scripts originales.
Puedes seguir usando `build_teaspeak.sh` directamente si prefieres control manual.
