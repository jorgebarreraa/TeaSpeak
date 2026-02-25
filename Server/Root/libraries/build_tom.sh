cd tommath/build
cmake .. -DCMAKE_CXX_FLAGS="-fPIC ${CXX_FLAGS}" -DCMAKE_C_FLAGS="-fPIC ${C_FLAGS}" -DCMAKE_BUILD_TYPE=RelWithDebInfo ${CMAKE_OPTIONS}
make ${CMAKE_MAKE_OPTIONS}
sudo make install
cd ../../tomcrypt/
chmod +x create_build.sh && ./create_build.sh
