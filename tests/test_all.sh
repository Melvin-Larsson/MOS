#!/bin/bash

if [ -z ${PREFIX+x} ]; then
    echo "PREFIX env variable not set"
    exit 1
fi

if ! make -C "$PREFIX/tests"; then
    echo "Failed to build"
    exit 1
fi


success=0

for testFile in $PREFIX/tests/bin/*.o; do
    printf "\033[0;37m"
    echo "Running $testFile"
    $testFile

    if [ $? -ne 0 ]; then
        success=1
    fi
done

printf "\033[0;37m"

exit $success
