#! /bin/bash

if [[ "$#" -lt 2 ]]; then
    echo "Usage: "$(basename "$0")" sourceFile outputFile [run | debug]"
    exit -1
fi

sourceFile=$1
outputFile=$2
debug=""
cFiles=""
libs="-lraylib -lX11"
warnings="-Wno-narrowing"
options=""

for opt in "$@"; do
    if [[ "$opt" == "run" ]]; then
        options="&& ./$2"
    elif [[ "$opt" == "debug" ]]; then
        debug="-g -o0"
    fi
done

cmd="g++ $debug $sourceFile $cFiles -o $outputFile $libs $warnings $options"
echo "Running $cmd..."
eval $cmd
