# Guía de Corrección de Errores de Compilación - TeaSpeak Server

## Contexto

Durante la compilación del servidor TeaSpeak en `/root/teaspeak-build/server/Server/Server/build`, se presentan dos errores principales:

1. **PermMapHelper** - Error de compilación (detalles no visibles en el tail del log)
2. **TeaLicenseServer** - Error de enlace con JsonCpp relacionado con `std::string_view`

---

## ERROR 1: JsonCpp string_view Compatibility (CRÍTICO)

### Descripción del Error
```
undefined reference to `Json::Value::operator[](std::basic_string_view<char, std::char_traits<char> >)'
undefined reference to `Json::Value::operator[](std::basic_string_view<char, std::char_traits<char> >) const'
```

### Causa
El código está compilado con C++11/14/17, y los literales de cadena en expresiones como `response["type"]` se convierten implícitamente a `std::string_view` en ciertos contextos. La versión de JsonCpp enlazada NO tiene sobrecargas de `operator[]` para `std::string_view`, solo para `std::string` y `const char*`.

### Archivo Afectado
`/root/teaspeak-build/server/Server/Server/license/server/WebAPI.cpp`

### Solución Automática

**Paso 1**: Ejecutar el script de corrección:

```bash
cd /root/teaspeak-build/server/Server/Server/license/server

# Crear backup
cp WebAPI.cpp WebAPI.cpp.backup_$(date +%Y%m%d_%H%M%S)

# Aplicar fixes con sed
sed -i 's/message\["\([^"]*\)"\]/message[std::string("\1")]/g' WebAPI.cpp
sed -i 's/response\["\([^"]*\)"\]/response[std::string("\1")]/g' WebAPI.cpp
sed -i 's/indexed_data\["\([^"]*\)"\]/indexed_data[std::string("\1")]/g' WebAPI.cpp
sed -i 's/history_data\["\([^"]*\)"\]/history_data[std::string("\1")]/g' WebAPI.cpp
sed -i 's/builder\["\([^"]*\)"\]/builder[std::string("\1")]/g' WebAPI.cpp

echo "Fix aplicado exitosamente a WebAPI.cpp"
```

**Paso 2**: Verificar los cambios:
```bash
# Ver un ejemplo de los cambios
grep -n 'response\[std::string' WebAPI.cpp | head -5
```

Deberías ver líneas como:
```cpp
response[std::string("type")] = "error";
response[std::string("code")] = "general";
```

### Solución Manual (Alternativa)

Si prefieres hacer cambios más selectivos, puedes editar manualmente WebAPI.cpp:

**Antes**:
```cpp
response["type"] = "error";
message["code"].asString()
```

**Después**:
```cpp
response[std::string("type")] = "error";
message[std::string("code")].asString()
```

---

## ERROR 2: PermMapHelper Compilation

### Diagnóstico

Para diagnosticar correctamente el error de PermMapHelper, necesitamos ver el log completo:

```bash
cd /root/teaspeak-build/server/Server/Server/build

# Limpiar build anterior
make clean

# Compilar y capturar todo el log
make -j4 2>&1 | tee /tmp/compile_complete.log

# Ver las primeras 300 líneas donde suelen aparecer los errores
head -300 /tmp/compile_complete.log
```

### Posibles Causas y Soluciones

#### Causa 1: Missing Headers / Include Paths

El archivo `helpers/PermMapGen.cpp` incluye:
```cpp
#include "log/LogUtils.h"
#include "Definitions.h"
#include "PermissionManager.h"
```

Estos headers deberían estar en `../shared/src/` o ser provistos por la biblioteca TeaSpeak enlazada.

**Solución**: Verificar que el directorio `shared` existe:
```bash
ls -la /root/teaspeak-build/server/Server/Server/../shared/src/
```

Si no existe, puede ser un symlink faltante. Verifica la estructura del build.

#### Causa 2: Dependency Build Failure

PermMapHelper depende de las bibliotecas estáticas:
- TeaSpeak
- TeaLicenseHelper
- TeaMusic

Si alguna de estas falla al compilarse, PermMapHelper también fallará.

**Solución**: Compilar solo PermMapHelper para ver el error específico:
```bash
cd /root/teaspeak-build/server/Server/Server/build
make PermMapHelper 2>&1 | tee /tmp/permmaphelper_error.log
cat /tmp/permmaphelper_error.log
```

#### Causa 3: Símbolos No Definidos

Puede haber símbolos no resueltos de las bibliotecas enlazadas.

**Solución Temporal (Workaround)**: Si PermMapHelper no es crítico para ejecutar el servidor, se puede deshabilitar temporalmente:

```bash
# Editar CMakeLists.txt y comentar el target PermMapHelper
# O compilar sin ese target:
cd /root/teaspeak-build/server/Server/Server/build
make TeaSpeak-Server TeaLicenseServer TeaSpeak-FileServer
```

---

## PLAN DE ACCIÓN RECOMENDADO

### Paso 1: Arreglar JsonCpp (Seguro y Necesario)

```bash
cd /root/teaspeak-build/server/Server/Server/license/server
cp WebAPI.cpp WebAPI.cpp.backup

# Aplicar fix
sed -i 's/message\["\([^"]*\)"\]/message[std::string("\1")]/g' WebAPI.cpp
sed -i 's/response\["\([^"]*\)"\]/response[std::string("\1")]/g' WebAPI.cpp
sed -i 's/indexed_data\["\([^"]*\)"\]/indexed_data[std::string("\1")]/g' WebAPI.cpp
sed -i 's/history_data\["\([^"]*\)"\]/history_data[std::string("\1")]/g' WebAPI.cpp
sed -i 's/builder\["\([^"]*\)"\]/builder[std::string("\1")]/g' WebAPI.cpp
```

### Paso 2: Intentar Recompilar

```bash
cd /root/teaspeak-build/server/Server/Server/build
make -j4 2>&1 | tee /tmp/compile3.log
```

### Paso 3: Analizar Resultado

#### Si la compilación tiene éxito:
```bash
echo "¡Compilación exitosa!"
ls -lh ../license/environment/TeaLicenseServer
ls -lh environment/TeaSpeak-Server
```

#### Si PermMapHelper aún falla:
```bash
# Ver solo errores de PermMapHelper
grep -A 20 "PermMapHelper" /tmp/compile3.log | head -50

# O compilar solo ese target para diagnóstico
make PermMapHelper 2>&1
```

### Paso 4: Diagnosticar PermMapHelper (si es necesario)

```bash
# Verificar estructura de directorios
ls -la /root/teaspeak-build/server/Server/Server/
ls -la /root/teaspeak-build/server/Server/Server/../shared/ 2>&1

# Verificar qué bibliotecas se compilaron
find /root/teaspeak-build/server/Server/Server/build -name "*.a" | grep -E "TeaSpeak|License|Music"
```

---

## VERIFICACIÓN FINAL

Una vez que la compilación tenga éxito:

```bash
# Verificar que los ejecutables existen
ls -lh /root/teaspeak-build/server/Server/Server/environment/TeaSpeak-Server
ls -lh /root/teaspeak-build/server/Server/Server/license/environment/TeaLicenseServer
ls -lh /root/teaspeak-build/server/Server/Server/file/environment/TeaSpeak-FileServer

# Verificar dependencias
ldd /root/teaspeak-build/server/Server/Server/environment/TeaSpeak-Server | head -20
```

---

## RESUMEN DE ARCHIVOS MODIFICADOS

Después de aplicar los fixes, los siguientes archivos habrán sido modificados:

1. **WebAPI.cpp** - Corrección de compatibilidad con JsonCpp
   - Ubicación: `/root/teaspeak-build/server/Server/Server/license/server/WebAPI.cpp`
   - Cambios: Conversión explícita de literales de cadena a `std::string`
   - Backup: `WebAPI.cpp.backup_<timestamp>`

---

## NOTAS IMPORTANTES

1. **No tocar converter.h**: El cambio que hiciste en converter.h probablemente no era necesario. El código original ya tenía el cast correcto.

2. **JsonCpp Version**: Este problema indica que la versión de JsonCpp enlazada no tiene soporte completo para C++17. Considera actualizar JsonCpp en el futuro.

3. **PermMapHelper**: Este ejecutable genera `permission_mapping.txt`. Si falla pero los servidores principales compilan, el sistema puede funcionar sin él.

4. **Build Clean**: Si los errores persisten después de aplicar los fixes, intenta:
   ```bash
   cd /root/teaspeak-build/server/Server/Server/build
   rm -rf *
   cmake ..
   make -j4
   ```

---

**Fecha**: 2025-11-25
**Autor**: Claude Code
**Estado**: Pendiente de implementación
