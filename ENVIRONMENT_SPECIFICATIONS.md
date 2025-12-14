# 🔧 Especificaciones Exactas del Entorno de Desarrollo

Este documento describe las versiones exactas de todas las herramientas y librerías usadas para compilar TeaSpeak exitosamente.

---

## 📍 Pregunta 1: ¿Ubicación Especial?

**NO** - El proyecto puede estar en cualquier ubicación del sistema.

**Recomendaciones:**
```bash
# Cualquiera de estas ubicaciones funciona:
/home/usuario/TeaSpeak          ✓
/opt/TeaSpeak                   ✓
/usr/local/src/TeaSpeak         ✓
~/proyectos/TeaSpeak            ✓
```

**Lo único importante:**
- Permisos de lectura/escritura en el directorio
- Espacio suficiente (~5GB para compilación completa)
- NO usar rutas con espacios: `/home/user/my projects/TeaSpeak` ✗

---

## 🖥️ Sistema Operativo Base

```
Distribución: Ubuntu 24.04.3 LTS (Noble Numbat)
Kernel: Linux 4.4.0
Arquitectura: x86_64 (AMD64)
```

**Sistemas compatibles:**
- ✅ Ubuntu 22.04 LTS o superior
- ✅ Debian 11 o superior
- ✅ Linux Mint 21 o superior
- ⚠️ Otras distribuciones: posibles pero no probadas

---

## 🛠️ Compiladores y Herramientas de Build

### GCC/G++ (CRÍTICO)
```
Versión: GCC 13.3.0
Paquete: gcc g++
```

**Por qué es importante:**
- GCC 13+ requiere `#include <cstdint>` explícito (ya corregido en el código)
- GCC 12 o inferior puede fallar con errores de headers
- GCC 14+ no probado pero debería funcionar

**Instalar:**
```bash
sudo apt install gcc g++ build-essential
```

### CMake
```
Versión: 3.28.3
Mínimo requerido: 3.10
```

**Instalar:**
```bash
sudo apt install cmake
```

### Make
```
Versión: GNU Make 4.3
```

**Instalar:**
```bash
sudo apt install make
```

### Git
```
Versión: 2.43.0
```

**Instalar:**
```bash
sudo apt install git
```

---

## 🔐 OpenSSL (MUY IMPORTANTE)

### Versión del Sistema
```
OpenSSL 3.0.13
Paquetes: libssl3t64, libssl-dev
```

**CRÍTICO:** TeaSpeak requiere OpenSSL 3.0.x específicamente
- ❌ OpenSSL 1.1.x NO funciona (libteaspeak_rtc requiere símbolos de 3.0)
- ✅ OpenSSL 3.0.x funciona perfectamente
- ⚠️ OpenSSL 3.1+ no probado

**Instalar:**
```bash
sudo apt install libssl-dev libssl3t64
```

**Verificar versión:**
```bash
openssl version
# Debe mostrar: OpenSSL 3.0.x
```

---

## 🗄️ MySQL Client Library

```
Versión: 8.0.44
Paquetes: libmysqlclient-dev, libmysqlclient21
```

**Instalar:**
```bash
sudo apt install libmysqlclient-dev
```

---

## 📦 Librerías del Sistema Requeridas

### Librerías esenciales
```bash
# Herramientas de desarrollo
sudo apt install pkg-config

# Compresión (requerida por varias librerías)
sudo apt install zlib1g-dev

# Herramientas adicionales
sudo apt install curl wget
```

---

## 🐍 Python (Para algunos scripts de build)

```
Versión: Python 3.11.14
```

**Instalar:**
```bash
sudo apt install python3 python3-dev
```

---

## 📋 Lista Completa de Paquetes a Instalar

```bash
# Una sola línea para copiar y pegar:
sudo apt update && sudo apt install -y \
  build-essential \
  gcc \
  g++ \
  cmake \
  make \
  git \
  libssl-dev \
  libmysqlclient-dev \
  pkg-config \
  zlib1g-dev \
  python3 \
  python3-dev \
  curl \
  wget
```

---

## 🔍 Verificación del Entorno

Ejecuta este script para verificar que todo está instalado correctamente:

```bash
#!/bin/bash

echo "🔍 Verificando entorno de desarrollo TeaSpeak"
echo "=============================================="

# Función para verificar comando
check_command() {
    if command -v $1 &> /dev/null; then
        version=$($1 --version 2>&1 | head -1)
        echo "✅ $1: $version"
        return 0
    else
        echo "❌ $1: NO INSTALADO"
        return 1
    fi
}

# Verificar comandos
check_command gcc
check_command g++
check_command cmake
check_command make
check_command git
check_command pkg-config

# Verificar OpenSSL
if command -v openssl &> /dev/null; then
    version=$(openssl version)
    if [[ $version == *"3.0"* ]]; then
        echo "✅ openssl: $version"
    else
        echo "⚠️  openssl: $version (Advertencia: Se requiere 3.0.x)"
    fi
else
    echo "❌ openssl: NO INSTALADO"
fi

# Verificar librerías
echo ""
echo "Verificando librerías del sistema:"
pkg-config --exists openssl && echo "✅ libssl-dev instalado" || echo "❌ libssl-dev NO instalado"
pkg-config --exists mysqlclient && echo "✅ libmysqlclient-dev instalado" || echo "❌ libmysqlclient-dev NO instalado"
pkg-config --exists zlib && echo "✅ zlib1g-dev instalado" || echo "❌ zlib1g-dev NO instalado"

echo ""
echo "=============================================="
echo "Verificación completa. Si ves ✅ en todos, estás listo!"
```

---

## 💾 Espacio en Disco Requerido

```
Código fuente:              ~500 MB
Librerías compiladas:       ~2 GB
Binarios compilados:        ~500 MB
Archivos temporales:        ~1 GB
-------------------------------------------
TOTAL RECOMENDADO:         ~5 GB libres
```

---

## ⚙️ Variables de Entorno Importantes

Durante la compilación, estas variables se configuran automáticamente:

```bash
export build_os_type=linux
export build_os_arch=amd64
export crypto_library_path=/home/user/TeaSpeak/Server/Root/libraries/openssl-prebuild/linux_amd64/lib
```

**NO** necesitas configurarlas manualmente si usas el script de compilación automática.

---

## 🚀 ¿Qué sigue?

Una vez que tengas este entorno configurado, usa el script maestro de instalación:

```bash
# El script install_teaspeak_complete.sh instalará AUTOMÁTICAMENTE:
# 1. Todas las dependencias del sistema
# 2. Clonará el repositorio con todos los submódulos
# 3. Compilará todas las librerías
# 4. Compilará TeaSpeak en modo STABLE
# 5. Verificará que todo funciona

bash install_teaspeak_complete.sh
```

---

## 📝 Notas Adicionales

### ¿Por qué Ubuntu 24.04?
- Tiene OpenSSL 3.0.13 por defecto
- GCC 13.3.0 con soporte C++17 completo
- Librerías MySQL 8.0 modernas
- Paquetes actualizados y estables

### ¿Puedo usar otra distribución?
Sí, pero asegúrate de que tenga:
- GCC 13 o superior
- OpenSSL 3.0.x (NO 1.1.x)
- CMake 3.10+
- MySQL client library 8.0+

### ¿Qué pasa si tengo OpenSSL 1.1?
El proyecto NO compilará. Necesitas actualizar a Ubuntu 24.04 o instalar OpenSSL 3.0 desde fuentes.

---

**Última actualización:** 2025-12-11
**Probado en:** Ubuntu 24.04.3 LTS x86_64
**Estado:** ✅ Completamente funcional
