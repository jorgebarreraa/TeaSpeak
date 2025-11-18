#!/usr/bin/env bash

BASEDIR=$(dirname "$0")
cd "${BASEDIR}"

file_paths=(
    "$(pwd ~)/../../Web-Client/shared/declarations"
    "$(pwd ~)/TeaSpeak/Web-Client/shared/declarations"
    "$(pwd ~)/../../TeaSpeak/Web-Client/shared/declarations"
    "app/dummy-declarations"
    #TODO Windows path
)
files=(
    "exports_app.d.ts;imports_shared.d.ts"
    "exports_loader_app.d.ts;imports_shared_loader.d.ts"
#   "exports_loader.d.ts;imports_shared_loader.d.ts"
)

support_rel_linking=$(ln --help 2>&1 | grep -e "--relative" >/dev/null && echo "1" || echo "0")
support_rel_linking=0

path_target="./modules/renderer/imports"
{
    mkdir -p "${path_target}"

    for path in "${file_paths[@]}"
    do
        path_found=1
        for file_mapping in "${files[@]}"
        do
            file_mapping=($(echo ${file_mapping} | tr ";" " "))
            file=${file_mapping[0]}

            if [[ ! -f "${path}/${file}" ]]; then
                path_found=0
                echo "path test ${path} failed to file ${file}"
                break
            fi
        done
        [[ path_found -eq 1 ]] || continue

        for file in "${files[@]}"
        do
            file_mapping=($(echo ${file} | tr ";" "\n"))
            src_file=${file_mapping[0]}
            dst_file=${file_mapping[1]}

            if [[ -e "${path_target}/${dst_file}" ]] || [[ -L "${path_target}/${dst_file}" ]]; then
                rm "${path_target}/${dst_file}"
            fi


            if [[ ${support_rel_linking} -ne 0 ]]; then
                ln -rs "${path}/${src_file}" "${path_target}/${dst_file}"
            else
                _source=$(realpath "${path}/${src_file}")
                _current_dir=$(pwd)
                cd ${path_target}
                [[ $? -ne 0 ]] && {
                    echo "Failed to enter target directory"
                    exit 1;
                }
                ln -s "${_source}" "${dst_file}"
                cd ${_current_dir}
            fi
            echo "Linking \"${path_target}/${dst_file}\" to \"${path}/${src_file}\""

            cp "${path}/${src_file}" "${path_target}/.copy_${dst_file}"
            echo "Create copy \"${path}/${src_file}\" to \"${path_target}/.copy_${dst_file}\""
        done
        break
    done
}

if [[ ${path_found} -eq 0 ]]; then
    echo "Could not import a link to shared imports. Trying copied import."

    for file in "${files[@]}"
    do
        file_mapping=($(echo ${file} | tr ";" "\n"))
        dst_file=${file_mapping[1]}

        if [[ -e "${path_target}/${dst_file}" ]] || [[ -L "${path_target}/${dst_file}" ]]; then
            echo "Hmm target file already exists even thou it hasn't been found yet... Deleting it!"
            rm "${path_target}/${dst_file}"
        fi

        if [[ ! -e "${path_target}/.copy_${dst_file}" ]]; then
            echo "Missing copy of file ${dst_file} because we cant find any valid link!"
            exit 1
        fi

        if [[ ${support_rel_linking} -ne 0 ]]; then
            ln -rs "${path_target}/.copy_${dst_file}" "${path_target}/${dst_file}"
        else
            _source=$(realpath "${path_target}/.copy_${dst_file}")
            _current_dir=$(pwd)
            cd ${path_target}
            [[ $? -ne 0 ]] && {
                echo "Failed to enter target directory"
                exit 1;
            }
            ln -s "${_source}" "${dst_file}"
            cd ${_current_dir}
        fi
        echo "Linking \"${path_target}/${dst_file}\" to \"${path_target}/.copy_${dst_file}\""
    done
    path_found=1
fi

if [[ path_found -eq 0 ]]; then
    echo "Failed to find UI imports"
    echo "Add your path to 'file_paths' and build the declarations first"
    exit 1
fi

exit 0