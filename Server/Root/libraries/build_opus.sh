cd opus
./autogen.sh
cd build
../configure
make -DCMAKE_CXX_FLAGS="${CXX_FLAGS}" -DCMAKE_C_FLAGS="${C_FLAGS}" ${MAKE_OPTIONS}
sudo make install
