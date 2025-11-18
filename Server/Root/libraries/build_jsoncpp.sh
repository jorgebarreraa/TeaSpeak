cd jsoncpp/build
cmake .. -DCMAKE_CXX_FLAGS="-std=c++11 -fPIC ${CXX_FLAGS}" -DCMAKE_C_FLAGS="${C_FLAGS}" -DCMAKE_BUILD_TYPE=RelWithDebInfo ${CMAKE_OPTIONS}
make ${CMAKE_MAKE_OPTIONS}
sudo make install
