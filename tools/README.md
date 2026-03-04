# TeaSpeak Console Tools

Scripts para interactuar con un servidor TeaSpeak en ejecución.

## Scripts Disponibles

### teaspeak-console.sh (Consola Interactiva)

Proporciona una consola interactiva completa para enviar comandos al servidor.

**Uso:**
```bash
./teaspeak-console.sh
```

**Características:**
- Lectura en tiempo real de las respuestas del servidor
- Detección automática del proceso del servidor
- Espera automática si el servidor acaba de iniciar
- Salir con `exit`, `quit` o Ctrl+C (el servidor sigue ejecutándose)

### teaspeak-command.sh (Comando Único)

Envía un solo comando al servidor y muestra la respuesta.

**Uso:**
```bash
./teaspeak-command.sh <comando>
```

**Ejemplos:**
```bash
./teaspeak-command.sh help
./teaspeak-command.sh "shutdown now Server restart"
./teaspeak-command.sh "reload config"
./teaspeak-command.sh meminfo
```

## Requisitos

- El servidor TeaSpeak debe estar ejecutándose
- Los named pipes deben estar habilitados (por defecto en `/tmp/teaspeak_{PID}_in.term`)

## Comandos Disponibles

Una vez conectado, puedes usar:

- `help` - Lista todos los comandos disponibles
- `shutdown now <razón>` - Apaga el servidor inmediatamente
- `shutdown <tiempo> <razón>` - Programa un apagado (ej: `shutdown 1h:30m Maintenance`)
- `shutdown info` - Muestra información sobre apagados programados
- `shutdown cancel` - Cancela un apagado programado
- `reload config` - Recarga la configuración del servidor
- `info` - Información del servidor
- `meminfo [basic|malloc|track|buffers]` - Información de memoria
- `memflush [db|buffer|alloc]` - Libera caché de memoria
- `passwd <password> <repeat>` - Cambia la contraseña de serveradmin
- `permgrant <ServerId> <GroupId> <PermName> <Grant>` - Otorga permisos
- `chat <serverId> <mode> <targetId> <mensaje>` - Envía mensaje de chat

## Troubleshooting

**"Error: TeaSpeak server is not running"**
- Asegúrate de que el servidor esté iniciado
- Ejecuta: `pgrep -f TeaSpeakServer` para verificar

**"Error: Input pipe not found"**
- El servidor puede no haber inicializado los pipes aún
- Espera unos segundos y vuelve a intentar
- Verifica que el servidor inició correctamente revisando los logs

**Los comandos no responden**
- Verifica que el servidor esté ejecutándose: `ps aux | grep TeaSpeakServer`
- Verifica que los pipes existan: `ls -la /tmp/teaspeak_*`

## Documentación Completa

Para más detalles, consulta `../CONSOLE_INTERACTIVE.md`
