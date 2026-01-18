# Patch JsonCpp source to disable string_view support (incompatible with C++11 build)
if [ -f jsoncpp/include/json/value.h ]; then
    sed -i 's/#define JSONCPP_HAS_STRING_VIEW 1/\/\/ #define JSONCPP_HAS_STRING_VIEW 1 \/\/ Disabled for C++11 compatibility/' jsoncpp/include/json/value.h
fi

cd jsoncpp/build
cmake .. \
    -DCMAKE_CXX_FLAGS="-std=c++11 -fPIC ${CXX_FLAGS}" \
    -DCMAKE_C_FLAGS="${C_FLAGS}" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    ${CMAKE_OPTIONS}
make ${CMAKE_MAKE_OPTIONS}
sudo make install
