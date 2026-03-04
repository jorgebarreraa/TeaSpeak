# 🚀 Guía de Instalación TeaSpeak en VPS

Esta guía te proporciona comandos paso a paso para instalar TeaSpeak desde GitHub en tu VPS, asegurándote de que estés alineado con todos los cambios más recientes.

## 📋 Cambios Aplicados

✅ **Problema Resuelto**: Error de validación de licencia `"Failed to validate instance integrity: Connection refused"`

**Solución**: Se ha deshabilitado la validación de licencia externa para evitar intentos de conexión a servidores de licencia no disponibles.

---

## 🔧 Comandos para tu VPS

### **Paso 1: Limpiar Instalación Anterior**

```bash
# Ir al directorio padre
cd /root

# OPCIÓN A: Hacer backup (recomendado)
mv TeaSpeak TeaSpeak.backup.$(date +%Y%m%d_%H%M%S)

# OPCIÓN B: Borrar completamente (si no necesitas el backup)
# rm -rf TeaSpeak
```

---

### **Paso 2: Clonar Repositorio desde GitHub**

```bash
# Clonar el repositorio
git clone https://github.com/jorgebarreraa/TeaSpeak.git

# Entrar al directorio
cd TeaSpeak

# Cambiar a la rama con las correcciones
git checkout claude/fix-install-script-ULBSP

# Verificar que estás en la rama correcta
git branch
```

**Salida esperada:**
```
* claude/fix-install-script-ULBSP
```

---

### **Paso 3: Verificar los Archivos**

```bash
# Ver los últimos commits
git log --oneline -5

# Verificar que tienes el commit más reciente
git log --oneline -1
```

**Deberías ver:**
```
bdfeb66f Fix: Disable license validation to prevent connection errors
```

---

### **Paso 4: Instalar Dependencias del Sistema**

```bash
# Actualizar repositorios
apt-get update

# Instalar dependencias necesarias
apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    libssl-dev \
    pkg-config
```

---

### **Paso 5: Compilar TeaSpeak**

```bash
# Ir al directorio de scripts
cd /root/TeaSpeak/Server/Server/scripts

# Dar permisos de ejecución
chmod +x build_teaspeak.sh

# Ejecutar el script de compilación
# Nota: Esto puede tomar 20-30 minutos
./build_teaspeak.sh
```

**El script hará:**
- ✅ Descargar y compilar todas las dependencias
- ✅ Compilar el servidor TeaSpeak
- ✅ Copiar archivos de GeoLocalización
- ✅ Configurar el entorno

---

### **Paso 6: Verificar la Compilación**

```bash
# Verificar que el binario se creó
ls -lh /root/TeaSpeak/Server/Server/server/cmake-build-release/TeaSpeakServer

# Deberías ver algo como:
# -rwxr-xr-x 1 root root 362M Mar 3 10:30 TeaSpeakServer
```

---

### **Paso 7: Iniciar el Servidor**

```bash
# Ir al directorio del servidor
cd /root/TeaSpeak/Server/Server/server/cmake-build-release

# Crear directorio de datos (si no existe)
mkdir -p environment/files

# Iniciar el servidor
./TeaSpeakServer
```

---

## ✅ Verificación Post-Instalación

Después de iniciar el servidor, **ya NO deberías ver** estos errores:

```
❌ [ERROR] GLOBL | Failed to validate instance integrity:
❌ [ERROR] GLOBL | unexpected disconnect: write error (Connection refused)
```

### **Lo que SÍ deberías ver:**

```
✅ [INFO]   GEN | Starting TeaSpeak-Server v1.5.6
✅ [INFO]   GEN | Starting music providers
✅ [INFO]  FILE | Started to listen on 0.0.0.0:30303
✅ [INFO] QUERY | Starting server on 0.0.0.0,[::]:10101
✅ [INFO]     1 | [Web] Starting server on 0.0.0.0,[::]:9987
```

### **Credenciales Generadas:**

El servidor generará automáticamente:

1. **Server Query** (para administración por consola):
   ```
   Username: serveradmin
   Password: [password generado]
   ```

2. **Token de Administrador** (usar en el cliente):
   ```
   Token: [token generado - úsalo una sola vez]
   ```

**⚠️ IMPORTANTE**: Guarda estas credenciales, se muestran solo una vez.

---

## 🌐 Puertos Utilizados

| Puerto | Protocolo | Uso |
|--------|-----------|-----|
| 9987 | UDP/TCP | Servidor de voz (WebRTC) |
| 10101 | TCP | Server Query (administración) |
| 30303 | TCP | Transferencia de archivos |

### **Configurar Firewall:**

```bash
# Permitir puertos necesarios
ufw allow 9987/tcp
ufw allow 9987/udp
ufw allow 10101/tcp
ufw allow 30303/tcp

# Verificar reglas
ufw status
```

---

## 🔄 Comandos de Mantenimiento

### **Actualizar a la última versión:**

```bash
cd /root/TeaSpeak
git fetch origin
git checkout claude/fix-install-script-ULBSP
git pull origin claude/fix-install-script-ULBSP
cd Server/Server/scripts
./build_teaspeak.sh
```

### **Ver logs en tiempo real:**

```bash
cd /root/TeaSpeak/Server/Server/server/cmake-build-release
tail -f logs/teaspeak_*.log
```

### **Detener el servidor:**

```bash
# Buscar el proceso
ps aux | grep TeaSpeakServer

# Detener gracefully (SIGTERM)
kill <PID>

# Forzar detención (solo si es necesario)
kill -9 <PID>
```

---

## 🐛 Solución de Problemas

### **Problema: Error de compilación**
```bash
# Limpiar build anterior
cd /root/TeaSpeak/Server/Server/server
rm -rf cmake-build-release
cd ../scripts
./build_teaspeak.sh
```

### **Problema: "Could not setup geoloc!"**
Este es solo un WARNING, no afecta la funcionalidad principal. El servidor usará bandera por defecto para IPs desconocidas.

### **Problema: Puerto ya en uso**
```bash
# Ver qué proceso usa el puerto
netstat -tulpn | grep 9987

# Detener el proceso anterior
kill <PID>
```

---

## 📝 Notas Importantes

1. **Validación de Licencia**:
   - ✅ DESHABILITADA para evitar errores de conexión
   - El servidor funcionará sin intentar conectarse a servidores externos

2. **GeoLocalización**:
   - ⚠️ Puede mostrar warning si no encuentra la base de datos
   - No es crítico, el servidor funciona sin ella

3. **Base de Datos**:
   - SQLite por defecto
   - Se crea automáticamente en el primer inicio

4. **Logs**:
   - Ubicación: `Server/Server/server/cmake-build-release/logs/`
   - Rotación automática por fecha

---

## 📞 Soporte

Si encuentras problemas:

1. Verifica que todos los comandos se ejecutaron sin errores
2. Revisa los logs del servidor
3. Asegúrate de que los puertos no estén bloqueados
4. Verifica que tienes espacio en disco suficiente (mínimo 2GB libres)

---

## ✨ Cambios Técnicos Aplicados

### **Archivo Modificado**: `Server/Server/server/src/lincense/LicenseService.cpp`

**Cambios:**
1. `initialize()`: Marca la validación como exitosa desde el inicio
2. `execute_tick()`: Deshabilitado para evitar intentos de conexión externa

**Resultado**: El servidor no intentará validar su licencia contra servidores externos, evitando los errores de "Connection refused".

---

**Última actualización**: 3 de Marzo, 2026
**Commit**: bdfeb66f - Fix: Disable license validation to prevent connection errors
