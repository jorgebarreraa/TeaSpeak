# Ubicación de Repositorios Preparados

Los repositorios `build-helpers` y `TeaSpeak-shared` con todos los cambios aplicados están ubicados en:

```
/home/user/TeaSpeak/github-migration/build-helpers/
/home/user/TeaSpeak/github-migration/TeaSpeak-shared/
```

## ¿Por qué no están en el repo?

Estos directorios contienen repositorios git completos (con su propio `.git/`) y no pueden ser commiteados directamente dentro de otro repo git sin convertirlos en submódulos.

## Cómo usarlos

Ejecuta el script de migración:

```bash
cd /home/user/TeaSpeak/github-migration
bash upload_to_github.sh
```

Este script:
1. Tomará los repos de estos directorios locales
2. Los subirá a tu cuenta de GitHub
3. Te mostrará las URLs resultantes

## Contenido

### build-helpers/
- **Commits:** 2 (original + documentación)
- **Branch:** master
- **Último commit:** c1e8c8a - Add CHANGES.md documenting fork modifications

### TeaSpeak-shared/
- **Commits:** 2 (fixes + documentación)
- **Branch:** 1.4.10-jorgebarreraa
- **Último commit:** ef7dfb3 - Add CHANGES.md documenting fork modifications for GCC 13.3.0

## Verificación

Puedes inspeccionar los repos localmente:

```bash
cd /home/user/TeaSpeak/github-migration/build-helpers
git log --oneline
git diff HEAD~1

cd /home/user/TeaSpeak/github-migration/TeaSpeak-shared
git log --oneline
git diff HEAD~1
```
