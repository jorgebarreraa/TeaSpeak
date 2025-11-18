#!/usr/bin/env bash

cd $(dirname $0)

SYMBOL_ROOT="build/symbols"
BINARY_PATHS=("build/linux_x64/teaclient_connection.node" "build/linux_x64/teaclient_crash_handler.node" "build/linux_x64/teaclient_dns.node" "build/linux_x64/teaclient_ppt.node" "build/exe/update-installer")

echo "Created dump symbols!"
for file in ${BINARY_PATHS[@]}; do
    if [[ ! -e ${file} ]]; then
        echo "Missing binary ${file}. Skipping file"
        continue
    fi

    echo "Generating symbols for ${file}"
    symbols="$(dump_syms ${file} 2>&1)"
    if [[ $? -ne 0 ]]; then
        echo "Failed to dump symbols for ${file}. Skipping file. Output:"
        echo "${symbols}"
        continue;
    fi

    symbol_info=$(echo "${symbols}" | head -n1)
    symbol_info_array=($symbol_info)
    echo "Symbol dump id: ${symbol_info_array[3]}; Dump name: ${symbol_info_array[4]}"
    symbol_path="${SYMBOL_ROOT}/${symbol_info_array[4]}/${symbol_info_array[3]}"
    symbol_file="${symbol_info_array[4]}.sym"
    echo "Saving symbols to ${symbol_path}/${symbol_file}"
    mkdir -p ${symbol_path}
    [[ $? -ne 0 ]] && {
        echo "Failed to create target dump path! Skipping file"
        continue
    }
    echo "${symbols}" > "${symbol_path}/${symbol_file}"
done
echo "All symbols have been created"

