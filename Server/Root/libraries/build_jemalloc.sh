#!/bin/bash

source ../build-helpers/build_helper.sh || { echo "Failed to include build helper script. Abort"; exit 1; }

library_path=jemalloc
requires_rebuild ${library_path}
[[ $? -eq 0 ]] && exit 0

# Generate the configure file
cd jemalloc
if ! [ -f configure ]; then
	autoconf
	check_err_exit ${library_path}  "Failed to create configure file"
	if ! [ -f configure  ]; then
		err_exit ${library_path}  "Failed to create configure file"
		exit 1
	fi
fi
cd ..

# Generate the build environment
generate_build_path "${library_path}"
if [[ -d ${build_path} ]]; then
    echo "Removing old build directory"
    rm -rf ${build_path}
fi
mkdir -p ${build_path}
check_err_exit ${library_path} "Failed to create build directory"
cd ${build_path}
check_err_exit ${library_path} "Failed to enter build directory"

# Building jemalloc
../../configure
check_err_exit ${library_path}  "Failed to create makefile"

(cat Makefile | sed 's/.*$(CC) $(DSO_LDFLAGS) $(call RPATH,$(RPATH_EXTRA)) $(LDTARGET) $+ $(LDFLAGS) $(LIBS) $(EXTRA_LDFLAGS).*/	$(CXX) $(DSO_LDFLAGS) $(call RPATH,$(RPATH_EXTRA)) $(LDTARGET) $+ $(LDFLAGS) $(LIBS) $(EXTRA_LDFLAGS)/') > tmp
rm Makefile
mv tmp Makefile
make build_lib_shared -j 12 LIBS="-pthread -static-libgcc -static-libstdc++ -fPIC" CFLAGS="-static-libgcc -static-libstdc++ -fPIC" CXXFLAGS="-static-libgcc -static-libstdc++ -fPIC"
#make -j 12 LIBS="-pthread -static-libgcc -static-libstdc++" CFLAGS="-fPIC" CXXFLAGS="-fPIC"
#make -j 12 LIBS="-pthread -lm -l/usr/lib/gcc/x86_64-linux-gnu/5/libstdc++.a" ${MAKE_OPTIONS} CFLAGS="-shared -static-libstdc++ -static-libgcc -fPIC" CXXFLAGS="-static-libstdc++ -static-libgcc -shared -fPIC"
check_err_exit ${library_path}  "Failed to build jemalloc"

sudo make install &>/dev/null
if [ $? -ne 0 ]; then #Workaround because the install fails
	exit 0
fi

#Link the final so
cd ..
if [[ -L libjemalloc.so.2 || -f libjemalloc.so.2 ]]; then
	echo "Removing old jemalloc link"
	rm libjemalloc.so.2
fi
ln -s ${build_os_type}_${build_os_arch}/lib/libjemalloc.so.2 .


cd ../../
set_build_successful ${library_path}

