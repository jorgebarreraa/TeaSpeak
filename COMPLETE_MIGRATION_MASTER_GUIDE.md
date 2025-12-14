# 🚀 Guía Maestra: Migración Total de TeaSpeak a GitHub Personal

Esta guía te llevará paso a paso para migrar **TODOS** los repositorios externos de TeaSpeak a tu cuenta de GitHub, dándote control total y permanente sobre el proyecto.

---

## 📊 Resumen Ejecutivo

### ¿Qué hace esta migración?

Migra **26 repositorios** de diferentes fuentes a tu GitHub:
- ✅ 13 repos de GitHub (otros usuarios)
- ✅ 9 repos de git.did.science (TeaSpeak/WolverinDEV)
- ✅ 2 repos de Google (chromium, boringssl)
- ✅ 2 repos ya preparados con tus fixes

### Beneficios

1. **Control Total**: Todos los repos bajo tu cuenta
2. **Permanencia**: Si los repos originales desaparecen, tú tienes copias
3. **Independencia**: No dependes de servidores externos
4. **Modificable**: Puedes hacer cambios a cualquier dependencia
5. **Reproducible**: Cualquiera puede clonar tu proyecto y funciona inmediatamente

---

## 🎯 Proceso Completo (3 Pasos)

```
┌─────────────────────┐
│ Paso 1: Obtener     │  ← 2 minutos
│ Token de GitHub     │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│ Paso 2: Migrar      │  ← 30-40 minutos (automático)
│ Todos los Repos     │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│ Paso 3: Actualizar  │  ← 5 minutos
│ Referencias         │
└─────────────────────┘
```

---

## 📝 Paso 1: Obtener Token de GitHub

### 1.1 Crear Token

1. Ve a: https://github.com/settings/tokens
2. Click en **"Generate new token (classic)"**
3. Nombre: `TeaSpeak Migration`
4. Permisos a seleccionar:
   - ✅ `repo` (todos los sub-permisos)
   - ✅ `delete_repo` (opcional, para limpiar si algo sale mal)
5. Click en **"Generate token"**
6. **COPIA EL TOKEN INMEDIATAMENTE** (solo se muestra una vez)

### 1.2 Configurar Token

En tu terminal:

```bash
export GITHUB_TOKEN="ghp_tu_token_aqui_muy_largo"
```

**⚠️ IMPORTANTE:** El token es sensible. No lo compartas ni lo commits.

Para verificar que está configurado:
```bash
echo $GITHUB_TOKEN
# Debe mostrar tu token
```

---

## 🔄 Paso 2: Migrar Todos los Repositorios

### 2.1 Ejecutar Script de Migración

```bash
cd /home/user/TeaSpeak/github-migration
bash migrate_all_repos_to_github.sh
```

### 2.2 Qué hace el script

El script hará lo siguiente **automáticamente**:

1. **Clonar** cada repositorio externo
2. **Crear** el repositorio en tu GitHub
3. **Subir** todo el código (todas las ramas, tags, historial)
4. **Mostrar** progreso en tiempo real
5. **Reportar** éxitos y errores al final

### 2.3 Tiempo Estimado

- ⏱️ **30-40 minutos** en total
- Depende de tu conexión de internet
- El script hace pausas para no saturar la API de GitHub

### 2.4 Lista de Repos que Migrará

<details>
<summary><b>Ver lista completa (26 repos)</b></summary>

**Repos de GitHub:**
1. jsoncpp
2. CXXTerminal
3. opus
4. opusfile
5. yaml-cpp
6. libevent
7. StringVariable
8. ed25519
9. protobuf
10. DataPipes
11. jemalloc
12. zstd
13. build-helpers (con tus fixes)

**Repos de git.did.science:**
14. Thread-Pool
15. tomcrypt
16. tommath
17. TeaSpeak-Server
18. spdlog
19. libnice-prebuild
20. glib2.0
21. openssl-prebuild
22. TeaDNS
23. TeaSpeak-shared (con tus fixes)

**Repos de Google:**
24. breakpad
25. boringssl

</details>

### 2.5 Monitoreo

Durante la ejecución verás:

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
[3/26] Migrando: opus
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  URL original: https://github.com/xiph/opus
  Rama: master

  Clonando repositorio...
  Creando repositorio en GitHub...
  ✓ Repositorio creado exitosamente
  Subiendo código a GitHub...
  ✓ Migración completada
  → https://github.com/jorgebarreraa/opus
```

---

## 🔗 Paso 3: Actualizar Referencias

Una vez que todos los repos están en tu GitHub:

### 3.1 Ejecutar Script de Actualización

```bash
cd /home/user/TeaSpeak/github-migration
bash update_all_submodule_references.sh
```

### 3.2 Qué hace el script

1. **Backup** del `.gitmodules` actual
2. **Reemplaza** todas las URLs para que apunten a tu GitHub
3. **Sincroniza** los submódulos
4. **Muestra** los cambios
5. **Pregunta** si quieres commitear y pushear

### 3.3 Revisar Cambios

El script mostrará algo como:

```diff
- url = https://github.com/WolverinDEV/build-helpers.git
+ url = https://github.com/jorgebarreraa/build-helpers.git

- url = https://git.did.science/TeaSpeak/libraries/tommath.git
+ url = https://github.com/jorgebarreraa/tommath.git
```

### 3.4 Confirmar

El script preguntará:

```
¿Deseas commitear estos cambios? (y/n): y
¿Deseas hacer push? (y/n): y
```

Responde `y` a ambas preguntas.

---

## ✅ Verificación Final

### Verificar que todo funciona:

```bash
# Clonar en directorio temporal
cd /tmp
rm -rf test-migration
git clone --recurse-submodules https://github.com/jorgebarreraa/TeaSpeak.git test-migration

cd test-migration/Server/Root

# Compilar en modo STABLE
export build_os_type=linux
export build_os_arch=amd64
bash build_teaspeak.sh stable
```

**Resultado esperado:**
- ✅ Todos los submódulos se clonan desde tu GitHub
- ✅ Compilación exitosa
- ✅ Todos los componentes se construyen correctamente

---

## 📚 Estructura Final

Después de la migración:

```
GitHub: jorgebarreraa/
├── TeaSpeak (repo principal)
│   └── .gitmodules → Apunta a tus repos
│
└── Dependencias (26 repos):
    ├── jsoncpp
    ├── CXXTerminal
    ├── opus
    ├── opusfile
    ├── yaml-cpp
    ├── libevent
    ├── StringVariable (con fixes -fPIC)
    ├── ed25519
    ├── protobuf
    ├── DataPipes
    ├── jemalloc
    ├── zstd
    ├── build-helpers (con targets ed25519)
    ├── Thread-Pool
    ├── tomcrypt
    ├── tommath
    ├── TeaSpeak-Server
    ├── spdlog
    ├── libnice-prebuild
    ├── glib2.0
    ├── openssl-prebuild
    ├── TeaDNS
    ├── TeaSpeak-shared (con fixes GCC 13.3.0)
    ├── breakpad
    └── boringssl
```

---

## 🔧 Compilación (Modo STABLE)

### Diferencia entre modos:

- **stable**: Build estándar, recomendado para producción
- **optimized**: Build optimizado, más rápido pero menos debugging info

### Compilar en modo STABLE:

```bash
cd /home/user/TeaSpeak/Server/Root
export build_os_type=linux
export build_os_arch=amd64
bash build_teaspeak.sh stable
```

---

## 🆘 Troubleshooting

### Error: "GITHUB_TOKEN no está configurado"

```bash
export GITHUB_TOKEN="tu_token"
```

### Error: "Authentication failed"

- Verifica que el token no haya expirado
- Regenera el token si es necesario
- Verifica que tiene permisos `repo`

### Error: "Repository already exists"

Es normal si ejecutas el script múltiples veces. El script continuará.

### Error al clonar algún repositorio

Algunos repos de Google pueden requerir configuración especial. Si un repo falla:

1. Clónalo manualmente:
   ```bash
   git clone URL_ORIGINAL nombre-repo
   cd nombre-repo
   git remote set-url origin https://github.com/jorgebarreraa/nombre-repo.git
   git push --mirror
   ```

2. Continúa con el resto

---

## 🔄 Sincronización Futura con Upstream

Si quieres actualizar desde los repos originales:

```bash
cd /ruta/al/repo

# Agregar upstream
git remote add upstream URL_ORIGINAL

# Fetch cambios
git fetch upstream

# Merge
git merge upstream/main  # o master, según corresponda

# Push a tu repo
git push origin main
```

---

## 📊 Comparación Antes/Después

| Aspecto | Antes | Después |
|---------|-------|---------|
| **Repos externos** | 26 repos en diferentes lugares | 26 repos en tu GitHub |
| **Control** | Ninguno | Total |
| **Si un repo desaparece** | Proyecto roto ❌ | Sigue funcionando ✅ |
| **Dependencias** | git.did.science, GitHub, Google | Solo tu GitHub |
| **Modificar libs** | Imposible | Cuando quieras |
| **Clonar proyecto** | Puede fallar | Siempre funciona |

---

## ⏱️ Tiempo Total Estimado

- **Paso 1:** 2 minutos (obtener token)
- **Paso 2:** 30-40 minutos (migración automática)
- **Paso 3:** 5 minutos (actualizar referencias)

**Total: ~45 minutos**

La mayor parte del tiempo el script trabaja solo. Puedes dejarlo corriendo y hacer otras cosas.

---

## 🎉 Resultado Final

Después de completar estos pasos:

✅ Tienes **26 repositorios** en tu GitHub
✅ TeaSpeak **completamente independiente**
✅ **Control total** de todas las dependencias
✅ **Proyecto portable** - funciona en cualquier parte
✅ **Futuro asegurado** - no depende de repos externos
✅ **Compilación en modo STABLE** configurada

---

## 📞 Soporte

Si algo sale mal:

1. Revisa la sección de Troubleshooting
2. Ejecuta con `set -x` para debug:
   ```bash
   bash -x migrate_all_repos_to_github.sh
   ```
3. Los backups están en:
   - `.gitmodules.backup`
   - `/home/user/TeaSpeak/github-migration/all-repos/`

---

**Preparado:** 2025-12-10
**Autor:** Claude AI Assistant
**Para:** Jorge Barrera (@jorgebarreraa)

**¡Suerte con la migración! 🚀**
