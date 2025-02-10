#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Invalid  number of arguments. Expected 'test_file.sh <filename>'"
    exit 1
fi

if [ -z $PREFIX ]; then
    echo "PREFIX not set"
    exit 1
fi

if ! make -C "$PREFIX/tests"; then
    echo "Failed to build"
    exit 1
fi

fileName=$(basename "$1")
executable="$PREFIX/tests/bin/${fileName%.c}.o"

echo "Testing $1"
"$executable"
