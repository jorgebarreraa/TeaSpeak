#!/usr/bin/env bash

#WARNING: Any spaces within the path will cause trouble!

#ldd -d
if [[ ! -f "$1" ]]; then
    echo "Missing target file ($1)"
    exit 1
fi

# This is a multidimensional array
# key /* library file */ => (library name; library file; dependencies ())
declare -A collected_libraries

# This is a one dimensional array
# key /* library file */ => use count
declare -A collected_libraries_use_count

declare -A libraries_of_result
function libraries_of {
    local buffer
    local index
    local data

    buffer=$(ldd -d "$1")
    index=0

    libraries_of_result=()

    IFS=$'\n'
    for line in ${buffer}; do
        index=$(($index + 1))
        [[ ${index} == 1 ]] && continue
        IFS=$' ' data=(${line})

        # We trim the leading and tailing white spaces
        _key=$(echo "${data[0]}" | sed -e 's/^[[:space:]]*//')
        _value=$(echo "${data[2]}" | sed -e 's/^[[:space:]]*//')
        libraries_of_result["${_key}"]="${_value}"
    done

    [[ $? -ne 0 ]] && return 1
    return 0
}

function print_lookup_stack {
    index_max=${#@}
    index_args="$@"

    #echo -n -e "\r\033[K"
    echo ""
    for (( index = 0; index < $index_max; ++index )); do
        arg=$(eval echo \$$((${index} + 1)))
        echo -n $(basename ${arg})
        [[ $(($index + 1)) -lt ${index_max} ]] && echo -n " => "
    done
    #sleep 1
}

declare -a libraries_of_deep_stack
function libraries_of_deep {
    local IFS
    local valid_libraries
    local inner_array
    local result_array

    libraries_of $1

    valid_libraries=()
    #echo "Gathered libraries for $1:"
    for library_name in "${!libraries_of_result[@]}"; do
        [[ -z "${libraries_of_result[$library_name]}" ]] && {
            #echo "  The dependency $library_name for $1 could not be resolved"
            continue
        }

        #echo "  $library_name at ${libraries_of_result[$library_name]}";
        valid_libraries+=("${libraries_of_result[$library_name]}")
    done

    IFS=$';' inner_array="${valid_libraries[*]}"
    IFS=$' ' result_array=("$(basename $1)" "$1" "$inner_array")
    collected_libraries[$1]="${result_array[@]}"

    libraries_of_deep_stack+=($1)
    print_lookup_stack ${libraries_of_deep_stack[@]}

    for library_path in "${valid_libraries[@]}"; do
        # echo "Looking up library path $library_path"
        [[ ! -z "${collected_libraries[$library_path]}" ]] && {
            #echo "Library $library_path already resolved"
            collected_libraries_use_count[$library_path]=$((${collected_libraries_use_count[$library_path]} + 1))
            continue
        }
        #echo "Resolving libraries for path $library_path"
        collected_libraries_use_count[$library_path]=1
        libraries_of_deep ${library_path}
        #library_name
    done
    unset 'libraries_of_deep_stack[${#libraries_of_deep_stack[@]}-1]';
}

libraries_of_deep $1
echo -e -n "\r\033[K" #Clear the stack

for key in "${!collected_libraries[@]}"; do
    IFS=$' ' library_data=(${collected_libraries[$key]})
    IFS=$';' libraries=(${library_data[2]})

    echo "Got library ${library_data[0]} (${library_data[1]}) directly used ${collected_libraries_use_count[$key]} times:"
    for library in "${libraries[@]}"; do
        echo "  $library"
    done
done