#!/bin/bash

#General modules should be set to their versions
git reset --hard
#git submodule foreach -q --recursive 'git reset --hard'
#git submodule update --init --recursive --force
#git submodule update --init --remote --recursive --force TeaSpeak

cd TeaSpeak
echo "Building TeaSpeak at version $(git rev-parse HEAD)"
cd ..

echo "Updating submodules"
#git submodule update --init --recursive
git submodule update --init --checkout --recursive -f
[[ $? -ne 0 ]] && {
    echo "Failed to update submodules. Aborting"
    exit 1
}
exit 0
