# Fix para PermMapHelper

## Problema

El ejecutable PermMapHelper no compila. Este helper se usa para generar el archivo `permission_mapping.txt`.

## Causa Probable

PermMapHelper (`helpers/PermMapGen.cpp`) incluye:
```cpp
#include <query/Command.h>
#include "log/LogUtils.h"
#include "Definitions.h"
#include "PermissionManager.h"
```

Estos headers están en:
- `TeaSpeakLibrary/src/query/Command.h`
- `TeaSpeakLibrary/src/log/LogUtils.h`
- `TeaSpeakLibrary/src/Definitions.h`
- `TeaSpeakLibrary/src/PermissionManager.h`

El problema es que el target PermMapHelper necesita tener acceso a los directorios de include de TeaSpeakLibrary.

## Soluciones Posibles

### Solución 1: Agregar include_directories para PermMapHelper

En `Server/Server/server/CMakeLists.txt`, después de la línea 183 (`add_executable(PermMapHelper helpers/PermMapGen.cpp)`), agregar:

```cmake
target_include_directories(PermMapHelper PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src
    ${LIBRARY_PATH}/TeaSpeakLibrary/src
)
```

### Solución 2: Verificar que TeaSpeakLibrary esté compilada

El target PermMapHelper depende del target TeaSpeak (static library) que debería incluir el código de TeaSpeakLibrary. Si TeaSpeak no se compila correctamente, PermMapHelper tampoco.

Verificar que:
1. El target TeaSpeak compile sin errores
2. TeaSpeak incluya todos los sources necesarios

### Solución 3: Compilar PermMapHelper por separado (workaround temporal)

Si PermMapHelper no es crítico para el servidor en sí, se puede excluir temporalmente del build:

```cmake
# En CMakeLists.txt, comentar las líneas 183-216
# add_executable(PermMapHelper helpers/PermMapGen.cpp)
# target_link_libraries(PermMapHelper ...)
```

**NOTA**: Esto significa que el archivo `permission_mapping.txt` no se generará automáticamente.

## Diagnóstico Necesario

Para determinar la causa exacta, necesitamos ver:
1. El error completo de compilación de PermMapHelper
2. Las primeras 200-300 líneas del log de compilación

**Comando para obtener el log completo**:
```bash
cd /root/teaspeak-build/server/Server/Server/build
make clean
make -j4 2>&1 | tee /tmp/compile_full.log
# Luego revisar el archivo /tmp/compile_full.log
```

## Conclusión

Sin ver el error completo, la causa más probable es que PermMapHelper no tiene acceso a los headers de TeaSpeakLibrary. La Solución 1 debería resolverlo.
