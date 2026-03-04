#!/usr/bin/env bash

cd "$(dirname $0)/../"

echo "Creating build for channel ${teaclient_deploy_channel}"
project_name="__build_teaclient"
source ../build-helpers/build_helper.sh

function install_npm() {
    begin_task "${project_name}_update" "Installing NPM"
    npm install --save-dev
    check_err_exit ${project_name} "Failed to install nodejs files!"
    npm run install-platform
    check_err_exit ${project_name} "Failed to install platform depend nodejs files!"

    npm update
    check_err_exit ${project_name} "Failed to update nodejs files!"
    end_task "${project_name}_update" "NPM installed"
}

function compile_scripts() {
    begin_task "${project_name}_tsc_sass" "Compiling TypeScript & SASS"
    #./build_declarations.sh
    #check_err_exit ${project_name} "Failed to build shared ui import declarations!"

    npm run compile-tsc -- -p modules/tsconfig_main.json
    check_err_exit ${project_name} "Failed to compile typescript main files!"
    npm run compile-tsc -- -p modules/tsconfig_renderer.json
    check_err_exit ${project_name} "Failed to compile typescript renderer files!"

    if [[ ${build_os_type} == "win32" ]]; then
        npm run compile-tsc -- -p installer/tsconfig_windows.json
        check_err_exit ${project_name} "Failed to compile typescript installer files!"
    else
        npm run compile-tsc -- -p installer/tsconfig_linux.json
        check_err_exit ${project_name} "Failed to compile typescript installer files!"
    fi

    npm run compile-sass
    check_err_exit ${project_name} "Failed to compile sass files!"
    end_task "${project_name}_tsc_sass" "TypeScript & SASS compiled"
    echo ""
}

function compile_native() {
    begin_task "${project_name}_native" "Compiling native extensions"


    local build_path="native/out/${build_os_type}_${build_os_arch}/"
    [[ -d ${build_path} ]] && rm -r ${build_path}
    mkdir -p ${build_path}
    check_err_exit ${project_name} "Failed to create build directory!"

    cd ${build_path}
    check_err_exit ${project_name} "Failed to enter build directory!"

    local _arguments=""
    [[ -n "$tearoot_cmake_module" ]] && _arguments="${_arguments} -DCMAKE_MODULE_PATH=\"$tearoot_cmake_module\""
    [[ -n "$tearoot_cmake_config" ]] && _arguments="${_arguments} -DCMAKE_PLATFORM_INCLUDE=\"$tearoot_cmake_config\""
    [[ -n "$traroot_library" ]] && _arguments="${_arguments} -DLIBRARY_PATH=\"$traroot_library\""

    local _cmake_generator=""
    [[ -n "${build_cmake_generator}" ]] && _cmake_generator="${build_cmake_generator}"
    [[ -z "${_cmake_generator}" && "${build_os_type}" == "win32" ]] && _cmake_generator=" -G\"Visual Studio 15 2017 Win64\""

    _command="cmake ../../ ${_cmake_generator} -DCMAKE_BUILD_TYPE=RelWithDebInfo ${_arguments}"
    echo "Executing cmake command $_command"

    eval ${_command}
    check_err_exit ${project_name} "Failed create build targets!"

    cmake --build "$(pwd)" --target update_installer -- ${CMAKE_MAKE_OPTIONS}
    check_err_exit ${project_name} "Failed build teaclient update installer!"

    cmake --build "$(pwd)" --target teaclient_connection -- ${CMAKE_MAKE_OPTIONS}
    check_err_exit ${project_name} "Failed build teaclient connection!"

    cmake --build "$(pwd)" --target teaclient_crash_handler -- ${CMAKE_MAKE_OPTIONS}
    check_err_exit ${project_name} "Failed build teaclient crash handler!"

    cmake --build "$(pwd)" --target teaclient_ppt -- ${CMAKE_MAKE_OPTIONS}
    check_err_exit ${project_name} "Failed build teaclient ppt!"

    cmake --build "$(pwd)" --target teaclient_dns -- ${CMAKE_MAKE_OPTIONS}
    check_err_exit ${project_name} "Failed to build teaclient dns!"

    end_task "${project_name}_native" "Native extensions compiled"
}

function package_client() {
    begin_task "${project_name}_package" "Packaging client"
    if [[ ${build_os_type} == "win32" ]]; then
        npm run build-windows-64 "${teaclient_deploy_channel}"
        check_err_exit ${project_name} "Failed to package client!"
    else
        npm run build-linux-64 "${teaclient_deploy_channel}"
        check_err_exit ${project_name} "Failed to package client!"
    fi
    end_task "${project_name}_package" "Client package created"
}

function deploy_client() {
    begin_task "${project_name}_package" "Deploying client"
    [[ -z ${teaclient_deploy_secret} ]] && {
        echo "Missing deploy secret. Dont deploy client!"
        return 0
    }
    [[ -z ${teaclient_deploy_channel} ]] && {
        echo "Missing deploy channel. Dont deploy client!"
        return 0
    }

    if [[ ${build_os_type} == "win32" ]]; then
        npm run package-windows-64 "${teaclient_deploy_channel}"
        check_err_exit ${project_name} "Failed to deploying client!"
    else
        npm run package-linux-64 "${teaclient_deploy_channel}"
        check_err_exit ${project_name} "Failed to deploying client!"
    fi
    end_task "${project_name}_package" "Client successfully deployed!"
}

#install_npm
#compile_scripts
#compile_native
package_client
deploy_client
