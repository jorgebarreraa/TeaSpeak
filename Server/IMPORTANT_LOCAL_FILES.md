# Archivos Importantes Locales (No Commiteados)

Este archivo documenta los directorios locales importantes que no están commiteados en el repositorio principal, pero que son necesarios para la compilación y migración.

## 📂 Directorios Locales

### 1. `Root/libraries/jsoncpp/`
- **Tipo:** Submódulo con contenido de build
- **URL original:** https://github.com/open-source-parsers/jsoncpp.git
- **Estado:** Contiene archivos compilados (.o, builds)
- **Acción requerida:** Será migrado a tu GitHub con el script

### 2. `Root/libraries/openssl-prebuild/`
- **Tipo:** Symlink a `/home/user/TeaSpeak/libraries/openssl-prebuild-master/`
- **Contenido:** Librerías OpenSSL 3.0 precompiladas
- **Estado:** Symlink específico del sistema
- **Acción requerida:** Será migrado a tu GitHub con el script

### 3. `Server/music/`
- **Tipo:** Directorio con submódulo git
- **URL original:** https://git.did.science/TeaSpeak/MusicBot.git
- **Estado:** Tiene su propio repositorio .git
- **Acción requerida:** Será migrado a tu GitHub con el script

### 4. `Server/shared/`
- **Tipo:** Submódulo git (TeaSpeakLibrary)
- **URL original:** https://git.did.science/TeaSpeak/TeaSpeakLibrary.git
- **Estado:** Ya preparado con fixes en `/home/user/TeaSpeak/github-migration/TeaSpeak-shared/`
- **Commit local:** e4ad735 (con fixes de compilación)
- **Acción requerida:** Ya está preparado para migración

### 5. `github-migration/TeaSpeak-shared/`
- **Tipo:** Repositorio git preparado para migración
- **Contenido:** Fork de TeaSpeakLibrary con fixes
- **Commits:** 2 (fixes + documentación)
- **Branch:** 1.4.10-jorgebarreraa
- **Acción requerida:** Se subirá con migrate_all_repos_to_github.sh

### 6. `github-migration/build-helpers/`
- **Tipo:** Repositorio git preparado para migración
- **Contenido:** Fork de build-helpers con ed25519 targets
- **Commits:** 2 (original + documentación)
- **Branch:** master
- **Acción requerida:** Se subirá con migrate_all_repos_to_github.sh

---

## 🔧 Por Qué No Están Commiteados

Estos directorios no están commiteados porque:

1. **Submódulos con .git propio:** Git no permite commitear repositorios anidados sin configurarlos como submódulos
2. **Symlinks específicos del sistema:** openssl-prebuild es un symlink local
3. **Archivos de build:** jsoncpp contiene archivos compilados temporales
4. **Repos preparados:** TeaSpeak-shared y build-helpers están listos para subir a GitHub

---

## ✅ Estado de Migración

| Directorio | Preparado | En GitHub | Script |
|------------|-----------|-----------|--------|
| jsoncpp | ✅ | Pendiente | migrate_all_repos_to_github.sh |
| openssl-prebuild | ✅ | Pendiente | migrate_all_repos_to_github.sh |
| music | ✅ | Pendiente | migrate_all_repos_to_github.sh |
| shared (TeaSpeak-shared) | ✅ | Pendiente | migrate_all_repos_to_github.sh |
| build-helpers | ✅ | Pendiente | migrate_all_repos_to_github.sh |

---

## 🚀 Próximos Pasos

Una vez que ejecutes la migración completa:

```bash
cd /home/user/TeaSpeak/github-migration
bash migrate_all_repos_to_github.sh
```

Todos estos directorios se convertirán en submódulos que apuntan a tu GitHub:

```
.gitmodules actualizado a:
- libraries/jsoncpp → https://github.com/jorgebarreraa/jsoncpp.git
- libraries/openssl-prebuild → https://github.com/jorgebarreraa/openssl-prebuild.git
- Server/music → https://github.com/jorgebarreraa/TeaSpeak-MusicBot.git
- Server/shared → https://github.com/jorgebarreraa/TeaSpeak-shared.git
- build-helpers → https://github.com/jorgebarreraa/build-helpers.git
```

---

## 📝 Verificación Post-Migración

Después de la migración, estos directorios:
- ✅ Existirán como submódulos git
- ✅ Apuntarán a tu GitHub
- ✅ Se clonarán automáticamente con `--recurse-submodules`
- ✅ Tendrán todos los cambios y fixes aplicados

---

**Última actualización:** 2025-12-10
**Estado:** Preparado para migración
**Script:** migrate_all_repos_to_github.sh
