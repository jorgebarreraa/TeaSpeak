# TeaSpeak Interactive Console

## Problema

El servidor TeaSpeak compilado no acepta comandos interactivos directamente desde stdin (entrada estándar). Los comandos como `help`, `shutdown`, `status`, etc. no funcionan cuando se escriben directamente en la consola.

## Causa

TeaSpeak usa un sistema de **named pipes** (FIFOs) para la comunicación con el terminal:
- Pipe de entrada: `/tmp/teaspeak_{PID}_in.term`
- Pipe de salida: `/tmp/teaspeak_{PID}_out.term`

Los comandos deben escribirse en el pipe de entrada, no en stdin del proceso.

## Solución

Se han creado scripts helper en el directorio `tools/`:
- `tools/teaspeak-console.sh` - Consola interactiva
- `tools/teaspeak-command.sh` - Envío de comandos únicos

### Uso

#### Opción 1: Consola Interactiva (Recomendado)

1. **Iniciar el servidor (en background o en otra terminal):**
   ```bash
   cd Server/Server/server/environment
   ./TeaSpeakServer &
   # o en otra terminal
   ```

2. **Conectar la consola interactiva (desde el directorio raíz del proyecto):**
   ```bash
   cd /ruta/a/TeaSpeak
   ./tools/teaspeak-console.sh
   ```

3. **Usar comandos:**
   ```
   > help
   Available commands:
     - end | shutdown
     - reload config
     - chat
     - info
     - permgrant
     - passwd
     - dummy_crash
     - memflush
     - meminfo

   > shutdown now Server maintenance
   Stopping instance
   ```

4. **Salir de la consola:**
   ```
   > exit
   ```
   El servidor continúa ejecutándose.

#### Opción 2: Comandos Únicos (Script)

Para enviar un solo comando sin entrar en modo interactivo:

```bash
./tools/teaspeak-command.sh help
./tools/teaspeak-command.sh "shutdown now Server maintenance"
./tools/teaspeak-command.sh "reload config"
./tools/teaspeak-command.sh meminfo
```

### Comandos disponibles

- `help` - Muestra la lista de comandos disponibles
- `shutdown now <reason>` - Apaga el servidor inmediatamente
- `shutdown <time> <reason>` - Programa un apagado (ej: `shutdown 1h:30m Maintenance`)
- `reload config` - Recarga la configuración
- `info` - Información del servidor
- `meminfo` - Información de memoria
- `memflush` - Libera memoria caché
- `passwd <new_pass> <repeat>` - Cambia la contraseña del serveradmin
- `exit` o `quit` - Sale de la consola (no apaga el servidor)

### Uso manual con pipes

También puedes enviar comandos directamente a los pipes:

```bash
# Encontrar el PID del servidor
PID=$(pgrep -f TeaSpeakServer | head -1)

# Enviar un comando
echo "help" > /tmp/teaspeak_${PID}_in.term

# Leer la respuesta
cat /tmp/teaspeak_${PID}_out.term
```

## Alternativa: Habilitar terminal compilado (CXXTerminal)

Para habilitar el terminal interactivo compilado en el binario (no recomendado por complejidad):

1. Compilar la biblioteca CXXTerminal (requiere resolver dependencias de libevent)
2. Cambiar en `Server/Server/server/main.cpp`:
   ```cpp
   #define ENABLE_TERMINAL 1  // cambiar de 0 a 1
   ```
3. Recompilar el servidor

**Nota**: Esta opción requiere resolver problemas de dependencias y linkeo, por lo que el script de consola es la solución recomendada.
