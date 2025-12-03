cd ed25519/build/
#cmake .. -DCMAKE_C_FLAGS="-fPIC -I../../boringssl/include/ -L../../boringssl/build/ssl/ -L../../boringssl/build/crypto/ ${C_FLAGS}" -DCMAKE_CXX_FLAGS="-fPIC ${CXX_FLAGS}" -DCMAKE_BUILD_TYPE=RelWithDebInfo ${CMAKE_OPTIONS}
cmake .. -DCMAKE_C_FLAGS="-fPIC ${C_FLAGS}" -DCMAKE_CXX_FLAGS="-fPIC ${CXX_FLAGS}" -DCMAKE_BUILD_TYPE=RelWithDebInfo ${CMAKE_OPTIONS}
make ${CMAKE_MAKE_OPTIONS}
sudo make install
