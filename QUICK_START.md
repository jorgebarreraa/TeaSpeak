# Quick Start: Aplicar Fixes de Compilación

## Para tu Servidor de Build (root@TeaspeakCompilando)

Ejecuta estos comandos en orden:

### 1. Aplicar Fix de WebAPI.cpp

```bash
cd /root/teaspeak-build/server/Server/Server/license/server

# Backup
cp WebAPI.cpp WebAPI.cpp.backup_final

# Aplicar fix (múltiples pasadas para casos anidados)
for i in {1..5}; do
  sed -i 's/\([a-zA-Z_][a-zA-Z0-9_]*\)\["\([^"]*\)"\]/\1[std::string("\2")]/g' WebAPI.cpp
  sed -i 's/\]\["\([^"]*\)"\]/][std::string("\1")]/g' WebAPI.cpp
done

# Verificar
echo "Conversiones aplicadas: $(grep -c 'std::string(' WebAPI.cpp)"
```

### 2. Aplicar Fix de CMakeLists.txt

```bash
# Editar el archivo
vi /root/teaspeak-build/server/Server/Server/license/CMakeLists.txt
```

**Buscar** (línea ~138):
```cmake
target_link_libraries(LicenseCLI TeaLicenseHelper)
```

**Reemplazar con**:
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

**O usar sed**:
```bash
cd /root/teaspeak-build/server/Server/Server/license

# Backup
cp CMakeLists.txt CMakeLists.txt.backup_final

# Crear archivo temporal con el nuevo contenido
cat > /tmp/licensecli_fix.txt << 'ENDFIX'
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
ENDFIX

# Aplicar usando sed
sed -i '/^target_link_libraries(LicenseCLI TeaLicenseHelper)$/r /tmp/licensecli_fix.txt' CMakeLists.txt
sed -i '/^target_link_libraries(LicenseCLI TeaLicenseHelper)$/d' CMakeLists.txt
```

### 3. Regenerar CMake y Compilar

```bash
cd /root/teaspeak-build/server/Server/Server/build

# IMPORTANTE: Regenerar CMake
cmake ..

# Compilar
make -j4 2>&1 | tee /tmp/compile_final.log
```

### 4. Verificar Resultado

```bash
# Ver últimas líneas del log
tail -50 /tmp/compile_final.log

# Verificar ejecutables creados
ls -lh ../license/environment/TeaLicenseServer
ls -lh ../license/environment/LicenseCLI
ls -lh environment/TeaSpeak-Server
```

## Resultado Esperado

Si todo funciona correctamente, deberías ver:

```
✓ TeaLicenseServer compilado exitosamente
✓ LicenseCLI compilado exitosamente
✓ TeaSpeak-Server compilado exitosamente
✓ TeaSpeak-FileServer compilado exitosamente
```

**Nota**: PermMapHelper puede fallar, pero no es crítico para ejecutar los servidores.

## Si Algo Sale Mal

### Error: "std::string was not declared in this scope"

Verifica que agregaste `<string>` al include si es necesario, o que el fix de sed se aplicó correctamente.

### Error: Todavía aparece "undefined reference to Json::Value::operator[]"

Ejecuta el loop de sed de nuevo con más iteraciones:
```bash
cd /root/teaspeak-build/server/Server/Server/license/server
for i in {1..10}; do
  sed -i 's/\([a-zA-Z_][a-zA-Z0-9_]*\)\["\([^"]*\)"\]/\1[std::string("\2")]/g' WebAPI.cpp
  sed -i 's/\]\["\([^"]*\)"\]/][std::string("\1")]/g' WebAPI.cpp
done
```

### Error: "threadpool::static not found"

Verifica que el CMakeLists.txt se modificó correctamente y que ejecutaste `cmake ..` antes de `make`.

---

**¿Preguntas?** Revisa `APPLY_FIXES_TO_BUILD.md` para instrucciones detalladas.
