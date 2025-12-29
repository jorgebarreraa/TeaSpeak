# 🔧 Repositorios de C/C++ Migrados y Patcheados

**Fecha:** 2025-12-29

Total de repositorios C/C++ migrados: **4**

---

## 📦 Repositorios Migrados

### 1. **tommath** ⭐ Librería Criptográfica
**URL Original:** https://git.did.science/TeaSpeak/libraries/tommath
**URL Migrada:** https://github.com/jorgebarreraa/tommath

**Descripción:** Librería de matemáticas de precisión arbitraria usada para criptografía.

**Cambios:**
- Migración directa sin cambios (código estable)
- Compila exitosamente en Linux

---

### 2. **tomcrypt** ⭐ Librería Criptográfica
**URL Original:** https://git.did.science/TeaSpeak/libraries/tomcrypt
**URL Migrada:** https://github.com/jorgebarreraa/tomcrypt

**Descripción:** Librería de criptografía portátil. Depende de tommath.

**Cambios:**
- Migración directa sin cambios
- Compila exitosamente en Linux
- Enlazada con tommath

---

### 3. **Thread-Pool** ⭐ Threading Library
**URL Original:** https://github.com/WolverinDEV/Thread-Pool
**URL Migrada:** https://github.com/jorgebarreraa/Thread-Pool

**Descripción:** Pool de hilos para operaciones concurrentes en el servidor.

**Cambios:**
- Migración directa sin cambios
- Compila exitosamente en Linux

---

### 4. **DataPipes** ⭐ Data Processing
**URL Original:** https://github.com/WolverinDEV/DataPipes
**URL Migrada:** https://github.com/jorgebarreraa/DataPipes
**Commit:** `3f799f4`

**Descripción:** Pipeline de procesamiento de datos para WebRTC y streaming.

**Fixes Aplicados:**
- **Fix Critical:** Agregado `#include <stdexcept>` en buffer.h
  - Movido fuera del bloque `#if defined(_MSC_VER)`
  - Necesario para `std::out_of_range` en Linux
  - Línea 166: `throw std::out_of_range(buffer);`

**Commit Message:**
```
Fix: Include stdexcept for Linux builds

- Move #include <stdexcept> outside _MSC_VER block
- Fixes 'out_of_range' is not a member of 'std' error on Linux
- stdexcept needed for std::out_of_range usage in buffer.h:166
```

---

## 📊 Resumen de Dependencias

```
tommath (standalone)
  └── tomcrypt (depende de tommath)

Thread-Pool (standalone)

DataPipes (standalone, usada por WebRTC)
```

## ✅ Estado de Compilación

| Librería    | Estado | Archivos Generados |
|-------------|--------|-------------------|
| tommath     | ✅ OK  | `libtommathStatic.a`, `libtommathShared.so` |
| tomcrypt    | ✅ OK  | `libtomcrypt.a` |
| Thread-Pool | ✅ OK  | `libThreadPoolStatic.a`, `libThreadPool.so` |
| DataPipes   | ✅ OK  | Con fix de stdexcept |

---

## 🔗 Integración con TeaSpeak

Estas librerías se clonan en:
```
/root/TeaSpeak/Server/Root/libraries/
├── tommath/out/linux_amd64/
├── tomcrypt/out/linux_amd64/
├── Thread-Pool/out/linux_amd64/
└── DataPipes/out/linux_amd64/
```

Y se enlazan durante la compilación de TeaSpeak Server usando CMake.

---

**Última actualización:** 2025-12-29 05:45 UTC
