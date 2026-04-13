#! /bin/bash

# Usage: build.sh sourceFile outputFile [run]

sourceFile=$1
outputFile=$2
cFiles=""
libs="-lraylib -lX11"
warnings="-Wno-narrowing"

options=""
if [ -v 3 ]; then
    if [ "$3" == "run" ]; then
        options="&& ./$2"
    fi
fi

cmd="g++ $sourceFile $cFiles -o $outputFile $libs $warnings $options"
eval $cmd
