#! /bin/bash

if [[ "$#" -lt 2 ]]; then
    echo "Usage: "$(basename "$0")" sourceFile outputFile [debug | run]"
    exit -1
fi

sourceFile=$1
outputFile=$2
compiler="g++"
optimize="-o4"
cFiles=""
libs="-lraylib -lX11"
warnings="-Wall"
options=""

for opt in "$@"; do
    if [[ "$opt" == "run" ]]; then
        options="&& ./$2"
    elif [[ "$opt" == "debug" ]]; then
        optimize="-g -o0"
    fi
done

cmd="$compiler $optimize $sourceFile $cFiles -o $outputFile $libs $warnings $options"
echo "Running $cmd..."
eval $cmd
