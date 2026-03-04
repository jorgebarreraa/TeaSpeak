# 📖 Guía de Uso del Instalador TeaSpeak

## 🚀 Formas de Ejecutar el Script

### **Opción 1: Modo Automático (Recomendado)**

```bash
./setup_teaspeak.sh
```

**¿Qué pasa?**
- **Primera vez**: Te pedirá usuario y token de GitHub, los guarda en `.teaspeak.conf`
- **Siguientes veces**: Usa automáticamente la configuración guardada (CERO prompts)

**Ejemplo de primera ejecución:**
```
╔═══════════════════════════════════════════════════════════╗
║         CONFIGURACIÓN INICIAL DE GITHUB                  ║
╚═══════════════════════════════════════════════════════════╝

Ingresa tu usuario de GitHub: jorgebarreraa
Ingresa tu GitHub token (ghp_xxxxx): ghp_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx

✓ Configuración guardada en: /root/TeaSpeak/.teaspeak.conf
```

**Ejemplo de segunda ejecución:**
```
✓ Usuario de GitHub configurado: jorgebarreraa
✓ Token de GitHub configurado (ghp_xxx...)

[INFO] Iniciando instalación...
```

---

### **Opción 2: Con Parámetros (Override)**

```bash
./setup_teaspeak.sh --github-user jorgebarreraa --github-token ghp_xxxxx
```

**¿Qué pasa?**
- Usa los parámetros proporcionados
- **Los guarda** automáticamente en `.teaspeak.conf` para futuras ejecuciones
- No te vuelve a pedir nada

**Útil cuando:**
- Quieres configurar todo en una sola línea
- Estás usando un script de automatización
- Quieres cambiar el usuario o token

---

### **Opción 3: Solo Usuario (Reutiliza Token)**

```bash
./setup_teaspeak.sh --github-user otro_usuario
```

**¿Qué pasa?**
- Usa el nuevo usuario
- **Reutiliza el token** del `.teaspeak.conf` si existe
- Si no hay token guardado, lo solicita

---

### **Opción 4: Con Build Type**

```bash
./setup_teaspeak.sh --build-type stable
./setup_teaspeak.sh --github-user jorgebarreraa --build-type optimized
```

**Build types disponibles:**
- `stable` - Producción estable (recomendado)
- `optimized` - Optimizado (más rápido)
- `debug` - Con debugging
- `nightly` - Nightly con optimizaciones

---

### **Opción 5: Saltar Pasos (Avanzado)**

```bash
# Saltar instalación de dependencias del sistema
./setup_teaspeak.sh --skip-deps

# Saltar compilación de librerías (usar cache)
./setup_teaspeak.sh --skip-libs

# Combinar opciones
./setup_teaspeak.sh --github-user jorgebarreraa --skip-deps --build-type stable
```

---

## 🔧 Archivo de Configuración

**Ubicación:** `/root/TeaSpeak/.teaspeak.conf`

**Contenido:**
```bash
# Configuración de TeaSpeak - Generado automáticamente
# NO COMPARTIR ESTE ARCHIVO (contiene token de autenticación)
SAVED_GITHUB_USER="jorgebarreraa"
SAVED_GITHUB_TOKEN="ghp_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
```

**Características:**
- ✅ Se crea automáticamente en la primera ejecución
- ✅ Permisos 600 (solo el dueño puede leer/escribir)
- ✅ En `.gitignore` (no se sube a GitHub)
- ✅ Se reutiliza automáticamente en futuras ejecuciones

**Cambiar configuración:**
```bash
# Opción 1: Editar manualmente
nano /root/TeaSpeak/.teaspeak.conf

# Opción 2: Eliminar y volver a ejecutar (te pedirá de nuevo)
rm /root/TeaSpeak/.teaspeak.conf
./setup_teaspeak.sh

# Opción 3: Pasar nuevos parámetros (sobrescribe automáticamente)
./setup_teaspeak.sh --github-user nuevo_usuario --github-token ghp_nuevo_token
```

---

## 📋 Todos los Parámetros Disponibles

| Parámetro | Descripción | Ejemplo |
|-----------|-------------|---------|
| `--github-user <usuario>` | Usuario de GitHub para repositorios | `--github-user jorgebarreraa` |
| `--github-token <token>` | Token de autenticación para repos privados | `--github-token ghp_xxxxx` |
| `--build-type <tipo>` | Tipo de compilación | `--build-type stable` |
| `--skip-deps` | Saltar instalación de dependencias | `--skip-deps` |
| `--skip-libs` | Saltar compilación de librerías | `--skip-libs` |
| `--help` | Mostrar ayuda | `--help` |

---

## 🎯 Casos de Uso Comunes

### **Caso 1: Primera instalación desde cero**
```bash
git clone https://github.com/jorgebarreraa/TeaSpeak.git
cd TeaSpeak
./setup_teaspeak.sh
# Te pedirá user/token, los guarda, compila todo
```

### **Caso 2: Reinstalación (ya tienes config guardada)**
```bash
cd /root/TeaSpeak
rm -rf Server/  # Limpiar todo
./setup_teaspeak.sh
# Usa automáticamente la config de .teaspeak.conf
# NO te pide nada, compila directo
```

### **Caso 3: Actualización (mantener compilación)**
```bash
cd /root/TeaSpeak
git pull
./setup_teaspeak.sh --skip-libs
# Salta compilación de librerías (usa cache)
# Solo recompila TeaSpeak Server
```

### **Caso 4: Cambiar de usuario de GitHub**
```bash
./setup_teaspeak.sh --github-user nuevo_usuario
# Usa nuevo usuario, guarda en config
# Reutiliza token anterior si existe
```

### **Caso 5: Compilación rápida para testing**
```bash
./setup_teaspeak.sh --build-type debug --skip-libs
# Build de debug sin recompilar librerías
```

---

## 🛡️ Seguridad

### **¿Es seguro guardar el token?**
Sí, siempre que:
- ✅ El archivo `.teaspeak.conf` tiene permisos 600 (solo tú puedes leerlo)
- ✅ El archivo está en `.gitignore` (no se sube a GitHub)
- ✅ Tu VPS está asegurado (acceso SSH con keys, no con passwords)

### **¿Qué pasa si alguien roba mi token?**
- Revoca el token en GitHub: https://github.com/settings/tokens
- Genera uno nuevo
- Actualiza la configuración: `./setup_teaspeak.sh --github-token ghp_nuevo_token`

---

## ❓ FAQ

**P: ¿Necesito pasar parámetros cada vez?**
R: No. Solo la primera vez, o si quieres cambiar la configuración.

**P: ¿Dónde se guarda mi configuración?**
R: En `/root/TeaSpeak/.teaspeak.conf` (no se sube a GitHub).

**P: ¿Puedo usar el script sin token?**
R: Sí, pero solo podrás clonar repositorios públicos. Presiona Enter cuando pida el token.

**P: ¿Cómo borro la configuración?**
R: `rm /root/TeaSpeak/.teaspeak.conf`

**P: ¿El token se sube a GitHub?**
R: No, está en `.gitignore`.

**P: ¿Puedo vender este script?**
R: Sí, el token NO está hardcodeado. Cada usuario configura el suyo.
