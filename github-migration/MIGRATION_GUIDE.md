# Guía de Migración de Submódulos a tu GitHub

Esta guía te ayudará a migrar los submódulos modificados (`build-helpers` y `TeaSpeak-shared`) a tu propia cuenta de GitHub para tener control total sobre ellos.

## 📦 Repos Preparados

En este directorio encontrarás dos repositorios listos para subir:

1. **build-helpers/** - Fork de WolverinDEV/build-helpers con cambios para ed25519
2. **TeaSpeak-shared/** - Fork de TeaSpeakLibrary con cambios de compilación

Cada uno tiene un archivo `CHANGES.md` que documenta todas las modificaciones.

---

## 🚀 Opción 1: Usando GitHub CLI (Recomendado)

### Prerrequisitos
```bash
# Instalar GitHub CLI si no lo tienes
# Ubuntu/Debian:
sudo apt install gh

# Autenticar
gh auth login
```

### Crear y Subir Repos Automáticamente

Ejecuta el script provisto:

```bash
cd /home/user/TeaSpeak/github-migration
bash upload_to_github.sh
```

Este script hará automáticamente:
1. Crear repos en tu GitHub
2. Cambiar el remote origin
3. Push de todos los commits
4. Mostrar las URLs de los nuevos repos

---

## 🔧 Opción 2: Manual (GitHub Web)

### Paso 1: Crear repo `build-helpers`

1. Ve a https://github.com/new
2. Nombre del repo: `build-helpers`
3. Descripción: "Fork of WolverinDEV/build-helpers with ed25519 CMake targets"
4. **NO** inicialices con README (ya tiene commits)
5. Click "Create repository"

6. En tu terminal:
```bash
cd /home/user/TeaSpeak/github-migration/build-helpers
git remote set-url origin https://github.com/jorgebarreraa/build-helpers.git
git push -u origin master
```

### Paso 2: Crear repo `TeaSpeak-shared`

1. Ve a https://github.com/new
2. Nombre del repo: `TeaSpeak-shared`
3. Descripción: "Fork of TeaSpeakLibrary with GCC 13.3.0 compilation fixes"
4. **NO** inicialices con README (ya tiene commits)
5. Click "Create repository"

6. En tu terminal:
```bash
cd /home/user/TeaSpeak/github-migration/TeaSpeak-shared
git remote set-url origin https://github.com/jorgebarreraa/TeaSpeak-shared.git
git push -u origin 1.4.10-jorgebarreraa
```

---

## 🔗 Paso 3: Actualizar Referencias de Submódulos

Una vez que hayas subido ambos repos, ejecuta:

```bash
cd /home/user/TeaSpeak/Server
bash update_submodules_to_fork.sh
```

Este script actualizará automáticamente:
- El submódulo `build-helpers` para apuntar a tu fork
- El submódulo `shared` para apuntar a tu fork
- Hará commit y push de los cambios

---

## ✅ Verificación

Después de la migración, verifica que todo funciona:

```bash
# Clonar el proyecto de nuevo (en otro directorio para probar)
cd /tmp
git clone --recurse-submodules https://github.com/jorgebarreraa/TeaSpeak.git test-teaspeak
cd test-teaspeak/Server/Root

# Compilar
export build_os_type=linux
export build_os_arch=amd64
bash build_teaspeak.sh optimized
```

Si compila correctamente, ¡la migración fue exitosa! 🎉

---

## 📊 Estado Después de la Migración

| Componente | Antes | Después |
|------------|-------|---------|
| **build-helpers** | WolverinDEV/build-helpers (sin cambios) | jorgebarreraa/build-helpers ✅ |
| **shared** | git.did.science (sin acceso) | jorgebarreraa/TeaSpeak-shared ✅ |
| **Control total** | ❌ | ✅ |
| **Cambios guardados** | Solo local | ✅ En GitHub |

---

## 🔄 Sincronizar con Upstream (Futuro)

Si los repos originales se actualizan y quieres traer cambios:

### Para build-helpers:
```bash
cd build-helpers
git remote add upstream https://github.com/WolverinDEV/build-helpers.git
git fetch upstream
git merge upstream/master
# Resolver conflictos si es necesario
git push origin master
```

### Para TeaSpeak-shared:
```bash
cd TeaSpeak-shared
git remote add upstream https://git.did.science/TeaSpeak/TeaSpeakLibrary.git
git fetch upstream
git merge upstream/1.4.10
# Resolver conflictos si es necesario
git push origin 1.4.10-jorgebarreraa
```

---

## 🆘 Troubleshooting

### Error: "Authentication failed"
```bash
# Re-autenticar GitHub CLI
gh auth logout
gh auth login
```

### Error: "Remote repository not found"
- Verifica que creaste los repos en GitHub
- Verifica que el nombre de usuario es correcto (jorgebarreraa)

### Error: "Updates were rejected"
```bash
# Force push (solo si estás seguro)
git push -f origin master
```

---

## 📝 Notas Adicionales

- **Privacidad:** Puedes hacer los repos privados si lo prefieres
- **Colaboración:** Puedes dar acceso a otros colaboradores
- **Issues:** Puedes habilitar issues para tracking de bugs
- **CI/CD:** Puedes configurar GitHub Actions para testing automático

---

*Preparado el: 2025-12-09*
*Autor: Claude AI Assistant*
