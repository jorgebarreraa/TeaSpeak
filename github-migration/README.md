# 🚀 Migración de Submódulos TeaSpeak a GitHub Personal

Este directorio contiene todo lo necesario para migrar los submódulos modificados de TeaSpeak a tu propia cuenta de GitHub.

## 📦 Contenido

```
github-migration/
├── README.md                    # Este archivo
├── MIGRATION_GUIDE.md           # Guía detallada paso a paso
├── upload_to_github.sh          # Script automático para subir repos
├── build-helpers/               # Repo listo con cambios de ed25519
│   ├── CHANGES.md               # Documentación de cambios
│   └── cmake/FindEd25519.cmake  # Archivo modificado
└── TeaSpeak-shared/             # Repo listo con fixes de compilación
    ├── CHANGES.md               # Documentación de cambios
    └── (múltiples archivos modificados)
```

## 🎯 Inicio Rápido

### Opción Automática (Recomendada)

```bash
# 1. Subir repos a tu GitHub
bash upload_to_github.sh

# 2. Actualizar referencias en tu proyecto TeaSpeak
cd /home/user/TeaSpeak/Server
bash update_submodules_to_fork.sh
```

### Opción Manual

Lee la guía completa:
```bash
cat MIGRATION_GUIDE.md
```

## ❓ ¿Por Qué Hacer Esto?

### Problema Anterior
- ❌ Submódulos apuntan a repos externos sin permisos de escritura
- ❌ Cambios críticos solo existen localmente
- ❌ Al clonar el proyecto, hay que re-aplicar cambios manualmente
- ❌ No hay control sobre las dependencias

### Después de la Migración
- ✅ Submódulos apuntan a TUS repos en GitHub
- ✅ Todos los cambios están guardados y versionados
- ✅ Al clonar, los cambios vienen automáticamente
- ✅ Control total sobre las dependencias
- ✅ Puedes hacer más cambios y pushearlos
- ✅ Otros pueden clonar tu proyecto y funciona inmediatamente

## 📊 Cambios Incluidos

### 1. build-helpers
**Archivo:** `cmake/FindEd25519.cmake`
- ✅ Agregados CMake imported targets (ed25519::static, ed25519::shared)
- **Commits:** 2 commits (original + documentación)

### 2. TeaSpeak-shared
**Archivos:** 10 archivos modificados
- ✅ Includes de `<cstdint>` para GCC 13.3.0
- ✅ Linking correcto de MySQL con zlib y OpenSSL
- ✅ Deshabilitado archivo de test problemático
- **Commits:** 2 commits (fixes + documentación)

## 🔧 Requisitos

### Para Opción Automática:
```bash
# GitHub CLI
sudo apt install gh

# Autenticar
gh auth login
```

### Para Opción Manual:
- Cuenta de GitHub activa
- Git configurado con tu email/nombre
- Navegador web para crear repos

## ✅ Verificación

Después de completar la migración:

```bash
# Clonar en directorio temporal para probar
cd /tmp
git clone --recurse-submodules https://github.com/jorgebarreraa/TeaSpeak.git test
cd test/Server/Root

# Compilar
export build_os_type=linux
export build_os_arch=amd64
bash build_teaspeak.sh optimized
```

Si compila sin errores, ¡la migración fue exitosa! 🎉

## 📚 Documentación Adicional

- **MIGRATION_GUIDE.md** - Guía completa con troubleshooting
- **build-helpers/CHANGES.md** - Cambios específicos de build-helpers
- **TeaSpeak-shared/CHANGES.md** - Cambios específicos de TeaSpeak-shared

## 🔄 Sincronización Futura

Si los repos upstream se actualizan:

```bash
# Para build-helpers
cd build-helpers
git remote add upstream https://github.com/WolverinDEV/build-helpers.git
git fetch upstream
git merge upstream/master

# Para TeaSpeak-shared
cd TeaSpeak-shared
git remote add upstream https://git.did.science/TeaSpeak/TeaSpeakLibrary.git
git fetch upstream
git merge upstream/1.4.10
```

## 🆘 Ayuda

Si tienes problemas:
1. Lee MIGRATION_GUIDE.md - tiene sección de troubleshooting
2. Verifica que estás autenticado en GitHub
3. Verifica que los nombres de repo son correctos
4. Verifica que tienes permisos de escritura

## 📝 Notas

- Los repos pueden ser públicos o privados (tú decides)
- Puedes dar acceso a colaboradores si lo necesitas
- Los cambios están documentados en CHANGES.md de cada repo
- Upstream original se puede agregar como remote para futuras actualizaciones

---

**Preparado:** 2025-12-09
**Por:** Claude AI Assistant
**Para:** jorge barreraa (@jorgebarreraa)
