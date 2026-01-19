# Desinstalar versión del sistema para evitar conflictos
echo "Desinstalando JsonCpp del sistema..."
sudo apt-get remove -y libjsoncpp-dev libjsoncpp25 2>/dev/null || true

# Reinstalar cmake si se eliminó como dependencia
if ! command -v cmake &> /dev/null; then
    echo "Reinstalando cmake..."
    sudo apt-get install -y cmake
fi

# Limpiar headers parcheados de sesiones anteriores
echo "Limpiando headers y librerías de JsonCpp previas..."
sudo rm -rf /usr/local/include/json
sudo rm -f /usr/local/lib/libjsoncpp*
sudo rm -rf /usr/local/lib/cmake/jsoncpp
sudo rm -rf /usr/local/lib/objects-*/jsoncpp_object
sudo rm -f /usr/local/lib/pkgconfig/jsoncpp.pc

# Limpiar source y rebuild de jsoncpp
echo "Restaurando código fuente limpio de JsonCpp..."
cd jsoncpp
git checkout . 2>/dev/null || true
git clean -fdx 2>/dev/null || true
cd ..

# Crear directorio de build limpio
rm -rf jsoncpp/build
mkdir -p jsoncpp/build

cd jsoncpp/build
echo "Compilando JsonCpp con C++17..."
cmake .. \
    -DCMAKE_CXX_FLAGS="-std=c++17 -fPIC ${CXX_FLAGS}" \
    -DCMAKE_C_FLAGS="${C_FLAGS}" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    ${CMAKE_OPTIONS}
make ${CMAKE_MAKE_OPTIONS}
sudo make install

# Actualizar cache del linker
sudo ldconfig

echo "JsonCpp instalado con C++17 en /usr/local/lib"
echo "Verificando símbolos string_view..."
nm -D /usr/local/lib/libjsoncpp.so | grep -c "string_view" || echo "ADVERTENCIA: No se encontraron símbolos string_view"
