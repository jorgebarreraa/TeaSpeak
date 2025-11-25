# Instrucciones para Aplicar los Fixes en tu Entorno de Build

## Resumen de Cambios

Los siguientes cambios se han aplicado al repositorio para corregir los errores de compilación:

1. **WebAPI.cpp**: Conversión de todos los accesos a Json::Value usando `std::string()`
2. **license/CMakeLists.txt**: Agregadas dependencias faltantes para LicenseCLI
3. **fix_json_webapi.py**: Script de Python para aplicar el fix automáticamente

## Cómo Aplicar en `/root/teaspeak-build/server/Server/Server/`

### Opción 1: Copiar Archivos Modificados (RECOMENDADA)

Si tienes acceso al repositorio en tu servidor de build:

```bash
# 1. Ir al directorio del repositorio
cd /ruta/al/repositorio/TeaSpeak

# 2. Asegurar que tienes los últimos cambios
git pull origin claude/review-docs-compile-01Y8KonAXkcZQd3RDs9UU9FZ

# 3. Copiar archivos modificados al build directory
cp Server/Server/license/server/WebAPI.cpp /root/teaspeak-build/server/Server/Server/license/server/
cp Server/Server/license/CMakeLists.txt /root/teaspeak-build/server/Server/Server/license/

# 4. Recompilar
cd /root/teaspeak-build/server/Server/Server/build
make -j4
```

### Opción 2: Aplicar el Script de Python

Si tienes el script fix_json_webapi.py:

```bash
# 1. Copiar el script
cp /ruta/al/repositorio/TeaSpeak/fix_json_webapi.py /tmp/

# 2. Modificar la ruta en el script para apuntar al build directory
# Editar /tmp/fix_json_webapi.py y cambiar:
# input_file = '/home/user/TeaSpeak/Server/Server/license/server/WebAPI.cpp'
# Por:
# input_file = '/root/teaspeak-build/server/Server/Server/license/server/WebAPI.cpp'

# 3. Ejecutar el script
python3 /tmp/fix_json_webapi.py

# 4. Aplicar el fix de CMakeLists.txt manualmente (ver abajo)
```

### Opción 3: Aplicar Manualmente (Si las opciones anteriores no funcionan)

#### Paso 1: Fix para WebAPI.cpp

```bash
cd /root/teaspeak-build/server/Server/Server/license/server

# Crear backup
cp WebAPI.cpp WebAPI.cpp.backup_manual

# Aplicar conversiones usando sed (múltiples pasadas para casos anidados)
for i in {1..5}; do
  sed -i 's/\([a-zA-Z_][a-zA-Z0-9_]*\)\["\([^"]*\)"\]/\1[std::string("\2")]/g' WebAPI.cpp
  sed -i 's/\]\["\([^"]*\)"\]/][std::string("\1")]/g' WebAPI.cpp
done

# Verificar que se aplicaron los cambios
grep -c 'std::string(' WebAPI.cpp
# Debería mostrar un número > 100
```

#### Paso 2: Fix para license/CMakeLists.txt

Editar `/root/teaspeak-build/server/Server/Server/license/CMakeLists.txt`:

Buscar la sección `target_link_libraries(LicenseCLI ...)` (alrededor de línea 138) y reemplazarla con:

```cmake
target_link_libraries(LicenseCLI
		TeaLicenseHelper
		threadpool::static
		TeaSpeak
		DataPipes::core::static
		openssl::ssl::static
		openssl::crypto::static
		libevent::core
		libevent::pthreads
		pthread
		dl
		rt
		stdc++fs
)
```

**Original** (ANTES):
```cmake
target_link_libraries(LicenseCLI TeaLicenseHelper)
```

**Modificado** (DESPUÉS):
```cmake
target_link_libraries(LicenseCLI
		TeaLicenseHelper
		threadpool::static
		TeaSpeak
		DataPipes::core::static
		openssl::ssl::static
		openssl::crypto::static
		libevent::core
		libevent::pthreads
		pthread
		dl
		rt
		stdc++fs
)
```

### Paso 3: Regenerar CMake y Recompilar

```bash
cd /root/teaspeak-build/server/Server/Server/build

# Regenerar archivos de build (necesario por cambios en CMakeLists.txt)
cmake ..

# Recompilar
make -j4 2>&1 | tee /tmp/compile_after_fixes.log
```

## Verificación de Éxito

Después de la compilación, verifica que los ejecutables se crearon:

```bash
# Verificar TeaLicenseServer
ls -lh /root/teaspeak-build/server/Server/Server/license/environment/TeaLicenseServer

# Verificar LicenseCLI
ls -lh /root/teaspeak-build/server/Server/Server/license/environment/LicenseCLI

# Verificar TeaSpeak-Server (si ya estaba compilando)
ls -lh /root/teaspeak-build/server/Server/Server/environment/TeaSpeak-Server
```

Si todos los archivos existen y tienen tamaño > 0, ¡la compilación fue exitosa!

## Errores Corregidos

### 1. JsonCpp string_view compatibility
**Error**:
```
undefined reference to `Json::Value::operator[](std::basic_string_view<char, std::char_traits<char> >)'
```

**Solución**: Convertir todos los accesos a Json::Value para usar `std::string()` explícitamente, forzando la sobrecarga `operator[](const std::string&)` en lugar de la inexistente `operator[](std::string_view)`.

### 2. LicenseCLI Thread-Pool linking
**Error**:
```
undefined reference to `threads::impl::ThreadBase::start(...)'
undefined reference to `threads::impl::FutureHandleData::triggerWaiters(...)'
```

**Solución**: Agregar `threadpool::static` y otras dependencias necesarias al target LicenseCLI en CMakeLists.txt.

## Problemas Conocidos

### PermMapHelper
Si PermMapHelper aún falla al compilar, es un problema separado que no afecta los servidores principales (TeaSpeak-Server, TeaLicenseServer, TeaSpeak-FileServer).

Para diagnosticar:
```bash
cd /root/teaspeak-build/server/Server/Server/build
make PermMapHelper 2>&1 | tee /tmp/permmaphelper_error.log
cat /tmp/permmaphelper_error.log
```

Para omitir temporalmente (si no es crítico):
```bash
# Compilar solo los targets necesarios
cd /root/teaspeak-build/server/Server/Server/build
make TeaSpeak-Server -j4
make TeaLicenseServer -j4
make TeaSpeak-FileServer -j4
```

## Soporte

Si encuentras errores adicionales después de aplicar estos fixes, verifica:

1. Que todos los archivos se copiaron correctamente
2. Que ejecutaste `cmake ..` antes de `make`
3. El log completo de compilación en `/tmp/compile_after_fixes.log`

---

**Última actualización**: 2025-11-25
**Commit**: 6db03f3 "Fix JsonCpp string_view compatibility and LicenseCLI linking errors"
