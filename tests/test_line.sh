#!/bin/bash

if [ $# -ne 2 ]; then
    echo "Invalid  number of arguments. Expected 'test_line.sh <filename> <line>'"
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


testPattern="^\s*TEST\s*(.\+,.\+){\?\s*"
testDeclarations=$(cat "$1" | grep -no "$testPattern")

testLine=""
testDeclaration=""
while read -r line ; do
    nr=$(echo "$line" | sed "s/^\(\\w*\).*/\1/g")

    if [[ ! -z $nr && $2 -ge $nr ]]; then
        testLine=$nr
        testDeclaration="$line"
    fi
done <<<"$testDeclarations"

if [[ -z $testLine ]]; then
    echo "No tests before line $2 in file: '$1'"
    exit 1
fi

testName=$(echo "$testDeclaration" | sed "s/^[0-9]\+:*\s*TEST\s*(\s*.\+,\s*\(.\+\)\s*)\s*{\?\s*$/\1/g")

fileName=$(basename "$1")
executable="$PREFIX/tests/bin/${fileName%.c}.o"

echo "Running '$testName'"
"$executable" "$testName"
