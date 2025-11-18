cmake_minimum_required(VERSION 3.1)

project(nan_ext VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

add_executable(nan_helper_tests test/main.cpp)

add_library(Nan__Helpers INTERFACE)
target_include_directories(Nan__Helpers INTERFACE
		include/
		#$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib1>
		#$<INSTALL_INTERFACE:${INSTALL_DI}/lib1>
)
add_library(Nan::Helpers ALIAS Nan__Helpers)