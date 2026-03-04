# Análisis de Errores TeaSpeak y Soluciones

## 📋 RESUMEN DE ERRORES ENCONTRADOS

### 1. 🟠 CRITICAL - Geolocalización
**Archivo:** `Server/Server/server/main.cpp:480-485`

```
[CRITICAL] GEN | Could not setup geoloc! Fallback to default flag!
[CRITICAL] GEN | Message: could not open file!
```

#### Causa Raíz
El servidor busca archivos de base de datos de geolocalización que no existen:
- `geoloc/IP2Location.CSV` (para IP2Location provider)
- `geoloc/IpToCountry.csv` (para Software77 provider)

#### Código Responsable
```cpp
// main.cpp líneas 472-486
if(!ts::config::geo::staticFlag) {
    if(ts::config::geo::type == geoloc::PROVIDER_SOFTWARE77)
        geoloc::provider = new geoloc::Software77Provider(ts::config::geo::mappingFile);
    else if(ts::config::geo::type == geoloc::PROVIDER_IP2LOCATION)
        geoloc::provider = new geoloc::IP2LocationProvider(ts::config::geo::mappingFile);

    if(geoloc::provider && !geoloc::provider->load(errorMessage)) {
        logCritical(LOG_GENERAL,"Could not setup geoloc! Fallback to default flag!");
        logCritical(LOG_GENERAL,"Message: {}", errorMessage);
        geoloc::provider = nullptr;
    }
}
```

#### Solución
**Opción 1 (Recomendada): Deshabilitar geolocalización en config.yml**
```yaml
geo:
  staticFlag: true  # Usar bandera estática por defecto
```

**Opción 2: Proveer archivos de geolocalización**
- Descargar base de datos IP2Location o Software77
- Colocarla en `server/environment/geoloc/IP2Location.CSV`

---

### 2. 🟡 WARNING - SQL Schema Mismatch
**Archivo:** `Server/Server/server/src/VirtualServerManager.cpp:425`

```
[WARNING] GLOBL | Failed to execute SQL command DELETE FROM `tokens` WHERE `serverId` = :sid
sql: no such column: serverId
```

#### Causa Raíz
**INCONSISTENCIA DE NOMENCLATURA**:
- **Base de datos** (migración v17→v18): usa `server_id` (snake_case)
- **Código**: busca `serverId` (camelCase)

#### Archivos Afectados
1. `Server/Server/server/src/VirtualServerManager.cpp:425`
   ```cpp
   execute_delete("DELETE FROM `tokens` WHERE `serverId` = :sid");
   ```

2. `Server/Server/server/src/DatabaseHelper.cpp:1242`
   ```cpp
   result = sql::command(this->sql, "DELETE FROM `tokens` WHERE `serverId` = :serverId AND `targetGroup` = :id",
                         variable{":serverId", server_id},
                         variable{":id", group_id}).execute();
   ```

#### Schema Correcto (desde SqlDataManager.cpp:588-598)
```sql
CREATE TABLE tokens_new(
  `token_id` INTEGER NOT NULL PRIMARY KEY [AUTO_INCREMENT],
  `server_id` INT,                    -- ← SNAKE_CASE
  `token` VARCHAR(32),
  `description` VARCHAR(255),
  `issuer_database_id` BIGINT,
  `max_uses` INT,
  `use_count` INT DEFAULT 0,
  `timestamp_created` INT,
  `timestamp_expired` INT
);
```

#### Solución
**Corregir las queries SQL para usar `server_id` en lugar de `serverId`**

**Archivo 1:** `Server/Server/server/src/VirtualServerManager.cpp`
```cpp
// Línea 425 - CAMBIAR DE:
execute_delete("DELETE FROM `tokens` WHERE `serverId` = :sid");

// A:
execute_delete("DELETE FROM `tokens` WHERE `server_id` = :sid");
```

**Archivo 2:** `Server/Server/server/src/DatabaseHelper.cpp`
```cpp
// Línea 1242 - CAMBIAR DE:
result = sql::command(this->sql, "DELETE FROM `tokens` WHERE `serverId` = :serverId AND `targetGroup` = :id",
                      variable{":serverId", server_id},
                      variable{":id", group_id}).execute();

// A:
result = sql::command(this->sql, "DELETE FROM `tokens` WHERE `server_id` = :serverId AND `targetGroup` = :id",
                      variable{":serverId", server_id},
                      variable{":id", group_id}).execute();
```

---

### 3. 🔴 ERROR - Network Read Event (Voice Server UDP)
**Archivo:** `Server/Server/server/src/server/VoiceServerSocket.cpp:107-109`

```
[ERROR] 1 | Failed to allocate network read event for voice server binding 0.0.0.0:9987
[ERROR] 1 | Failed to allocate network read event for voice server binding :::9987
```

#### Causa Raíz
`event_new()` de **libevent** está retornando `nullptr` al intentar crear eventos de red para el protocolo de voz UDP.

#### Código Responsable
```cpp
// VoiceServerSocket.cpp líneas 96-119
const auto& network_loop = serverInstance->network_event_loop();
const auto network_event_count = std::min(network_loop->loop_count(),
                                          ts::config::threads::voice::events_per_server);

for(size_t index{0}; index < network_event_count; index++) {
    auto events = std::make_unique<NetworkEvents>(this);
    events->event_read = network_loop->allocate_event(
        this->file_descriptor,
        EV_READ | EV_PERSIST,
        VoiceServerSocket::network_event_read,
        &*events,
        &read_use_list);

    if(!events->event_read) {  // ← AQUÍ FALLA
        logError(server_id, "Failed to allocate network read event for voice server binding {}",
                 net::to_string(this->address_));
        continue;
    }
    // ...
}
```

#### Posibles Causas Técnicas
1. **Problema con libevent**: La librería libevent no está funcionando correctamente con sockets UDP
2. **event_base_new() falló**: El event_base no se inicializó correctamente
3. **Límites del sistema**: File descriptors, memoria, o límites de eventos

#### Investigación Adicional Necesaria
**Verificar inicialización de libevent:**

```cpp
// GlobalNetworkEvents.cpp líneas 22-38
bool NetworkEventLoop::initialize() {
    while(this->event_loops.size() < this->event_loop_size) {
        auto event_loop = new EventLoop{this->event_loop_id_index++};
        event_loop->event_base = event_base_new();  // ← ¿Esto funcionó?
        if(!event_loop->event_base) {
            logError(LOG_GENERAL, "Failed to allocate new event base.");
            delete event_loop;
            return false;
        }
        // ...
    }
    return true;
}
```

#### Soluciones Propuestas

**Solución 1: Verificar libevent compilado con el proyecto**
Asegurarse de que libevent del proyecto se esté usando correctamente:
```bash
cd /home/user/TeaSpeak
ldd Server/Root/TeaSpeak/server/environment/TeaSpeakServer | grep event
```

**Solución 2: Agregar logs de depuración**
Modificar `GlobalNetworkEvents.cpp` para agregar más información:
```cpp
event* NetworkEventLoop::allocate_event(int fd, short events, event_callback_fn callback,
                                       void *callback_data, NetworkEventLoopUseList **use_list) {
    // ... código existente ...

    auto event = event_new(event_loop->event_base, fd, events, callback, callback_data);
    if(!event) {
        logError(LOG_GENERAL, "event_new() failed for fd={}, events={}, event_base={}",
                 fd, events, (void*)event_loop->event_base);
        return nullptr;
    }
    // ...
}
```

**Solución 3: Verificar configuración de threads**
El servidor intenta crear 4 eventos por servidor (`events_per_server: 4`). Si todos fallan, puede ser un problema de configuración:
```yaml
# En config.yml
threads:
  voice:
    events_per_server: 1  # Reducir a 1 para probar
  network_events: 2       # Verificar este valor
```

**Solución 4: Comparar con instalación original**
Comparar las librerías libevent entre tu compilación y la versión original de TeaSpeak.

---

### 4. 🔴 ERROR - Instance Integrity Validation
**Archivo:** `Server/Server/server/src/lincense/LicenseService.cpp:204-206`

```
[ERROR] GLOBL | Failed to validate instance integrity:
[ERROR] GLOBL | unexpected disconnect: write error (Connection refused)
```

#### Causa Raíz
El servidor intenta conectarse a `license.teamspeak.cl:27786` para validar la licencia/integridad de la instancia.

#### Código Responsable
```cpp
// LicenseService.cpp líneas 109-112
#ifdef DO_LOCAL_REQUEST
    auto license_host = gethostbyname(strobf("localhost").c_str());
#else
    auto license_host = gethostbyname(strobf("license.teamspeak.cl").c_str());  // ← AQUÍ
#endif
```

```cpp
// LicenseService.cpp líneas 204-206
logError(LOG_INSTANCE, strobf("Failed to validate instance integrity:").string());
logError(LOG_INSTANCE, error);
```

```cpp
// LicenseService.cpp línea 296
this->handle_check_fail(strobf("unexpected disconnect: ").string() + message);
```

#### Comportamiento
- Se ejecuta cada ~1 minuto
- Si es **licencia Premium** y falla muchas veces → **servidor se detiene**
- Si es **versión Demo/Gratis** → solo muestra el error (no crítico)

#### Solución
**Esta validación requiere una licencia oficial de TeamSpeak o modificaciones más profundas.**

**Opción 1: Servidor de licencias mock local (requiere modificación del código)**
- Comentar/desactivar la validación de licencia
- Compilar con `-DDO_LOCAL_REQUEST` y crear servidor mock local

**Opción 2: Aceptar el error**
- El error NO es crítico si no tienes licencia Premium
- El servidor funciona normalmente con este error

---

## 🎯 PRIORIDADES DE CORRECCIÓN

### ✅ ALTA PRIORIDAD (Afecta funcionalidad)
1. **ERROR #2 - SQL tokens**: Fácil de corregir, 2 cambios de código
2. **ERROR #3 - Network events**: Crítico para protocolo de voz UDP nativo

### 🟨 MEDIA PRIORIDAD (Afecta UX)
3. **ERROR #1 - Geolocalización**: Solo configuración, no afecta operación

### ⬜ BAJA PRIORIDAD (Informativo)
4. **ERROR #4 - Licencia**: Esperado sin licencia oficial, no bloquea funcionalidad básica

---

## 📝 PASOS SIGUIENTES

### Para ERROR #2 (SQL tokens) - INMEDIATO
1. Modificar `VirtualServerManager.cpp` línea 425
2. Modificar `DatabaseHelper.cpp` línea 1242
3. Recompilar
4. Probar

### Para ERROR #3 (Network events) - INVESTIGACIÓN
1. Verificar logs de inicialización de libevent
2. Comparar librerías con versión original
3. Probar reducir `events_per_server` a 1
4. Revisar si es problema de compilación de libevent

### Para ERROR #1 (Geoloc) - CONFIGURACIÓN
1. Agregar `staticFlag: true` en `config.yml`
2. Reiniciar servidor

### Para ERROR #4 (Licencia) - OPCIONAL
1. Ignorar si no tienes licencia Premium
2. O implementar servidor de licencias mock local

---

## 🔍 INFORMACIÓN ADICIONAL NECESARIA

Para continuar con la investigación del ERROR #3 (Network events), necesitamos:
1. Comparar con la instalación original de TeaSpeak que funciona
2. Ver qué versión de libevent usa el original
3. Verificar si hay parches adicionales necesarios para libevent
4. Revisar los logs de compilación de libevent en tu instalación

**¿Tienes acceso a una instalación original funcional de TeaSpeak para comparar?**
