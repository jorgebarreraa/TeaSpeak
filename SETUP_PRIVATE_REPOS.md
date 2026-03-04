# 🔐 Configuración de Repositorios Privados para Venta con Token

## 📌 Resumen del Sistema

Este proyecto está configurado con un **sistema de autenticación por token** que permite:
- ✅ Vender acceso temporal mediante tokens de GitHub
- ✅ Repositorios privados que solo son accesibles con token válido
- ✅ Validación automática al ejecutar el instalador
- ✅ Cache local del token para comodidad del usuario

## 🎯 Repositorios Requeridos

Para que el sistema funcione completamente, necesitas tener estos **3 repositorios privados** bajo tu cuenta de GitHub:

1. **`jorgebarreraa/TeaSpeak`** ✅ (Ya existe)
2. **`jorgebarreraa/TeaSpeakLibrary`** ⚠️ (Necesita ser creado/forkeado)
3. **`jorgebarreraa/TeaMusic-Providers`** ⚠️ (Necesita ser creado/forkeado)

## 📦 Cómo Crear los Repositorios Faltantes

### Opción 1: Forkear los Repositorios Originales

```bash
# Forkear TeaSpeakLibrary
gh repo fork https://github.com/TeaSpeak/TeaSpeakLibrary --clone=false

# Forkear TeaMusic-Providers
gh repo fork https://github.com/TeaSpeak/TeaMusic-Providers --clone=false

# Convertirlos a privados
gh repo edit jorgebarreraa/TeaSpeakLibrary --visibility private
gh repo edit jorgebarreraa/TeaMusic-Providers --visibility private
```

### Opción 2: Manualmente desde GitHub

1. Ve a los repositorios originales:
   - https://github.com/TeaSpeak/TeaSpeakLibrary
   - https://github.com/TeaSpeak/TeaMusic-Providers

2. Click en **"Fork"** en la esquina superior derecha

3. Después de forkear, ve a **Settings** → **General** → **Change visibility** → **Private**

## 🔑 Modelo de Negocio con Tokens

### Cómo Funciona:

1. **Cliente compra acceso:**
   - Le proporcionas un token de GitHub con acceso temporal a tus repos privados
   - El token puede tener una fecha de expiración

2. **Cliente instala TeaSpeak:**
   ```bash
   ./setup_teaspeak.sh --github-user jorgebarreraa --github-token ghp_xxxxx
   ```

3. **Validación automática:**
   - El script valida que el token tenga acceso a los 3 repositorios
   - Si el token es inválido/expirado, la instalación se detiene con mensaje claro
   - El token se guarda localmente para futuras reinstalaciones

4. **Expiración del acceso:**
   - Cuando el token expire, el cliente no podrá reinstalar/actualizar
   - Deberá renovar su licencia para obtener un nuevo token

### Crear Tokens para Clientes:

```bash
# Opción 1: Usando GitHub CLI
gh auth token

# Opción 2: Manualmente
# 1. Ve a: https://github.com/settings/tokens
# 2. "Generate new token (classic)"
# 3. Selecciona scope: "repo" (Full control of private repositories)
# 4. Opcional: Establece una fecha de expiración
# 5. Copia el token generado (ghp_xxxxx)
```

⚠️ **IMPORTANTE:** Cada cliente debe tener su propio token único para poder rastrear/revocar accesos individuales.

## 🛡️ Seguridad

### Lo que el Sistema Hace:

✅ Valida el token antes de iniciar la instalación
✅ Verifica acceso a todos los repos requeridos
✅ Guarda el token localmente con permisos 600
✅ Cachea el token en git credential helper (24 horas)
✅ Detecta tokens expirados y solicita uno nuevo
✅ Mensajes claros cuando el acceso falla

### Lo que Debes Hacer:

1. **Mantener los repos privados:**
   ```bash
   # Verificar que sean privados
   gh repo view jorgebarreraa/TeaSpeakLibrary --json visibility
   ```

2. **Crear tokens con expiración:**
   - Tokens de 30 días para licencia mensual
   - Tokens de 365 días para licencia anual
   - Tokens sin expiración para licencias permanentes

3. **Revocar tokens cuando sea necesario:**
   ```bash
   # Listar tokens activos
   gh auth status

   # Revocar desde: https://github.com/settings/tokens
   ```

## 📝 Archivo de Configuración

El instalador guarda el token en `.teaspeak.conf`:

```bash
# Configuración de TeaSpeak - Generado automáticamente
# ⚠️  NO COMPARTIR ESTE ARCHIVO (contiene token de autenticación privado)
SAVED_GITHUB_USER="jorgebarreraa"
SAVED_GITHUB_TOKEN="ghp_xxxxx"
```

**Permisos:** `-rw------- (600)` - Solo el propietario puede leer/escribir

## 🎨 Mensajes para el Cliente

El sistema muestra mensajes profesionales:

```
╔═══════════════════════════════════════════════════════════╗
║       🔐 AUTENTICACIÓN DE TEASPEAK REQUERIDA             ║
╚═══════════════════════════════════════════════════════════╝

Este software requiere autenticación para acceder a los
repositorios privados necesarios para la instalación.

Para obtener un token de acceso válido:

  🔹 Si ya tienes una licencia/acceso:
     Usa el token que se te proporcionó al momento de la compra

  🔹 Si necesitas adquirir acceso:
     Contacta con el proveedor del software
```

## ✅ Verificación del Sistema

Para probar que todo funciona:

```bash
# Test con token válido
./setup_teaspeak.sh --github-user jorgebarreraa --github-token ghp_xxxxx --help

# Debe mostrar:
# ✓ Usuario configurado: jorgebarreraa
# ✓ Token configurado (ghp_xxx...)
# ✓ Acceso verificado: jorgebarreraa/TeaSpeak
# ✓ Acceso verificado: jorgebarreraa/TeaSpeakLibrary
# ✓ Acceso verificado: jorgebarreraa/TeaMusic-Providers
# ✓ Token válido - Acceso verificado a todos los repositorios
```

## 📞 Soporte

Si encuentras problemas, verifica:

1. ✅ Los 3 repositorios existen y son privados
2. ✅ El token tiene scope "repo" completo
3. ✅ El token no ha expirado
4. ✅ El token tiene acceso a los 3 repositorios

---

**Estado Actual:**
- ✅ Sistema de autenticación implementado
- ✅ Validación automática funcionando
- ⚠️ Falta crear/forkear: TeaSpeakLibrary y TeaMusic-Providers como privados
