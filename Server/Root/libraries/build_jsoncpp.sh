# Desinstalar versión del sistema para evitar conflictos
echo "Desinstalando JsonCpp del sistema..."
sudo apt-get remove -y libjsoncpp-dev libjsoncpp25 2>/dev/null || true

cd jsoncpp/build
cmake .. \
    -DCMAKE_CXX_FLAGS="-std=c++17 -fPIC ${CXX_FLAGS}" \
    -DCMAKE_C_FLAGS="${C_FLAGS}" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_INSTALL_PREFIX=/usr \
    ${CMAKE_OPTIONS}
make ${CMAKE_MAKE_OPTIONS}
sudo make install

# Actualizar cache del linker
sudo ldconfig

echo "JsonCpp instalado con C++17 en /usr/lib"
