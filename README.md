# 🎵 TeaSpeak Server - Instalación Automática Completa

![TeaSpeak](https://img.shields.io/badge/TeaSpeak-Server-blue)
![Ubuntu](https://img.shields.io/badge/Ubuntu-24.04-orange)
![OpenSSL](https://img.shields.io/badge/OpenSSL-3.0-green)
![Status](https://img.shields.io/badge/Status-Production%20Ready-success)

**Fork completo de TeaSpeak Server con instalación y compilación 100% automática**

---

## 🚀 Instalación Rápida (3 pasos)

```bash
# 1. Clonar repositorio
git clone --recurse-submodules https://github.com/jorgebarreraa/TeaSpeak.git
cd TeaSpeak

# 2. Ejecutar instalador automático
bash install_teaspeak_complete.sh

# 3. ¡Listo! (15-30 minutos de espera)
```

El script instalará AUTOMÁTICAMENTE:
- ✅ Todas las dependencias del sistema
- ✅ GCC 13+, OpenSSL 3.0, MySQL, etc.
- ✅ Compilará todas las librerías
- ✅ Compilará TeaSpeak en modo STABLE
- ✅ Verificará que todo funciona

---

## 📖 Documentación

### Para Empezar

| Documento | Descripción |
|-----------|-------------|
| **[RESPUESTAS_RAPIDAS.md](RESPUESTAS_RAPIDAS.md)** | ⭐ **EMPIEZA AQUÍ** - Respuestas a las 3 preguntas principales |
| **[install_teaspeak_complete.sh](install_teaspeak_complete.sh)** | Script de instalación automática completa |
| **[ENVIRONMENT_SPECIFICATIONS.md](ENVIRONMENT_SPECIFICATIONS.md)** | Especificaciones técnicas detalladas del entorno |

### Información Adicional

| Documento | Descripción |
|-----------|-------------|
| [MIGRATION_COMPLETE.md](MIGRATION_COMPLETE.md) | Detalles de la migración de repositorios |
| [Server/Root/COMPILACION_AUTOMATICA.md](Server/Root/COMPILACION_AUTOMATICA.md) | Guía de compilación manual |
| [Server/Root/compile_teaspeak_auto.sh](Server/Root/compile_teaspeak_auto.sh) | Script de solo compilación (sin instalación de deps) |

---

## ❓ Preguntas Frecuentes

### 1. ¿Ubicación especial requerida?
**NO** - Puedes instalar TeaSpeak en cualquier directorio.

### 2. ¿Qué versiones necesito?
- **Ubuntu 24.04 LTS** (recomendado)
- **GCC 13.3.0** o superior
- **OpenSSL 3.0.13** (CRÍTICO - NO funciona con 1.1.x)
- **CMake 3.28.3**
- **MySQL Client 8.0.44**

Ver detalles completos en [ENVIRONMENT_SPECIFICATIONS.md](ENVIRONMENT_SPECIFICATIONS.md)

### 3. ¿Hay un script automático?
**SÍ** - `install_teaspeak_complete.sh` lo hace TODO automáticamente.

---

## 🎯 Características

- ✅ **100% Independiente** - Todos los submódulos en cuenta personal
- ✅ **Compilación Automática** - Un solo comando para instalar todo
- ✅ **Modo STABLE** - Compilado con optimizaciones de producción
- ✅ **Todas las Features** - TeaMusic, FFmpeg, YouTube, WebRTC habilitados
- ✅ **Documentación Completa** - Guías paso a paso
- ✅ **Fixes Aplicados** - GCC 13, OpenSSL 3.0, -fPIC, etc.

---

## 📋 Requisitos del Sistema

```
Sistema:     Ubuntu 24.04 LTS (recomendado)
RAM:         Mínimo 4GB
Disco:       ~5GB libres
CPU:         2+ cores (más rápido con más cores)
Conexión:    Internet para descargar dependencias
```

---

## 🔧 Uso del Script de Instalación

### Instalación estándar
```bash
bash install_teaspeak_complete.sh
```

### Instalación en directorio específico
```bash
bash install_teaspeak_complete.sh /opt/TeaSpeak
```

### ¿Qué hace el script?

1. Verifica sistema operativo
2. Instala dependencias (gcc, openssl, mysql, etc.)
3. Verifica versiones correctas
4. Clona repositorio con submódulos
5. Compila librerías (StringVariable, jsoncpp, libevent)
6. Configura entorno
7. Compila TeaSpeak en modo STABLE
8. Verifica binarios

**Tiempo estimado:** 15-30 minutos

---

## 📦 Submódulos Incluidos

Todos los repositorios externos migrados a cuenta personal:

- jsoncpp, CXXTerminal, opus, yaml-cpp
- libevent, StringVariable, ed25519, protobuf
- jemalloc, zstd, Thread-Pool, DataPipes
- tomcrypt, tommath, spdlog
- boringssl (598MB), breakpad
- TeaSpeak-Server, TeaSpeak-shared
- build-helpers (con fixes ed25519)
- Y más...

**Total:** 25 repositorios migrados ✅

Ver detalles en [MIGRATION_COMPLETE.md](MIGRATION_COMPLETE.md)

---

## 🏃 Ejecutar TeaSpeak

Después de la instalación:

```bash
cd TeaSpeak/Server/Root/TeaSpeak/Server/server/out/linux_amd64
./TeaSpeakServer
```

---

## 🔄 Recompilar en el Futuro

```bash
cd TeaSpeak/Server/Root
bash compile_teaspeak_auto.sh
```

---

## 🆘 Soporte

- **Issues:** https://github.com/jorgebarreraa/TeaSpeak/issues
- **Logs:** `/tmp/teaspeak_compile.log`
- **Documentación:** Ver archivos `.md` en el repositorio

---

## 📝 Notas de la Migración

- ✅ 25/25 repositorios migrados exitosamente
- ✅ Todas las referencias de submódulos actualizadas
- ✅ Proyecto 100% independiente de fuentes externas
- ✅ Todos los fixes aplicados (GCC 13, OpenSSL 3.0, -fPIC)
- ✅ Documentación completa incluida

---

## 📄 Licencia

Ver licencias originales en cada submódulo.

---

## 🙏 Créditos

- **TeaSpeak Server:** Proyecto original
- **Migración y Automatización:** Jorge Barrera (@jorgebarreraa)
- **Fecha:** 2025-12-10/11

---

**¿Listo para empezar?**

```bash
bash install_teaspeak_complete.sh
```

**¡Disfruta de TeaSpeak! 🎉**
