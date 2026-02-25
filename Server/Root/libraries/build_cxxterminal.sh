cd CXXTerminal/build/
event_path=$(realpath ../../event/build)
cmake .. -DCMAKE_CXX_FLAGS="${CXX_FLAGS}" -DCMAKE_C_FLAGS="${C_FLAGS}" -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DNO_EMBETTED_LIBEVENT=1 -DLibevent_DIR=${event_path} ${CMAKE_OPTIONS}
make ${CMAKE_MAKE_OPTIONS}
sudo make install
