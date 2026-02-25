cd StringVariable/build/
cmake .. -DCMAKE_CXX_FLAGS="${CXX_FLAGS} -fPIC" -DCMAKE_C_FLAGS="${C_FLAGS}" -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" ${CMAKE_OPTIONS}
make ${CMAKE_MAKE_OPTIONS}
sudo make install
