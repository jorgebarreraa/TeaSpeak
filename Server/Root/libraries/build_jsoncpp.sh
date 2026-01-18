cd jsoncpp/build
cmake .. \
    -DCMAKE_CXX_FLAGS="-std=c++17 -fPIC ${CXX_FLAGS}" \
    -DCMAKE_C_FLAGS="${C_FLAGS}" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DJSON_USE_STD_STRING_VIEW=ON \
    ${CMAKE_OPTIONS}
make ${CMAKE_MAKE_OPTIONS}
sudo make install
