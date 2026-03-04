#!/usr/bin/env bash

library_path="mysqlconnector"
requires_rebuild ${library_path}
[[ $? -eq 0 ]] && exit 0

cd ${library_path}
HEADER=$(cat cdk/foundation/connection_openssl.cc)
if ! [[ "${HEADER:0:34}" == "#if !defined(SSL_ERROR_WANT_ASYNC)" ]]; then
	echo "Creating new one!"
	echo "#if !defined(SSL_ERROR_WANT_ASYNC)
	#define SSL_ERROR_WANT_ASYNC 0xFFEEDDCC
#endif
#if !defined(SSL_ERROR_WANT_ASYNC_JOB)
	#define SSL_ERROR_WANT_ASYNC_JOB 0xFFEECCDD
#endif
`cat cdk/foundation/connection_openssl.cc`" > cdk/foundation/connection_openssl.cc
fi
cd ..

cmake_build ${library_path} -DCMAKE_CXX_FLAGS="-static-libgcc -static-libstdc++ -I`pwd`/boringssl/include/" -DWITH_JDBC=ON -DWITH_SSL="`pwd`/boringssl/" -DOPENSSL_ROOT_DIR="`pwd`/boringssl/" -DCMAKE_BUILD_TYPE=RelWithDebInfo
check_err_exit ${library_path} "Failed to build mysqlconnector!"
set_build_successful ${library_path}
