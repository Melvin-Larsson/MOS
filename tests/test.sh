#!/bin/bash

#Params: filename

if [ $# -ne 1 ]; then
    echo "Error";
    exit 1
fi

if [ ! -f $1 ]; then
    echo "Error: '$1' is not a file"
    exit 1
fi

testDeclarations=$(cat "$1" | grep -o "^\s*TEST\s*(.\+,.\+){\?\s*")
testGroup=$(echo "$testDeclarations" | sed "s/^\s*TEST\s*(\s*//g" | sed "s/,.\+//g")
testName=$(echo "$testDeclarations" | sed "s/^\s*TEST\s*(\s*.\+,\s*//g" | sed "s/\s*).*//g")

ignoredTestCount=$(cat "$1" | grep -o "^\s*IGNORE_TEST\s*(.\+,.\+){\?\s*" | wc -l );

testSetupDeclaration=$(cat "$1" | grep -o "^\s*TEST_GROUP_SETUP\s*(.\+){\?\s*")
testTeardownDeclaration=$(cat "$1" | grep -o "^\s*TEST_GROUP_TEARDOWN\s*(.\+){\?\s*")
testSetupGroups=$(echo "$testSetupDeclaration" | sed "s/^\s*TEST_GROUP_SETUP\s*(\s*//g" | sed "s/).*//g")
testTeardownGroups=$(echo "$testTeardownDeclaration" | sed "s/^\s*TEST_GROUP_TEARDOWN\s*(\s*//g" | sed "s/).*//g")

testList="static Test testArray[] = {"
testDeclarations=""

while IFS= read -r group; do
    testDeclarations+="void testSetup_${group}(void); "
done <<< "$testSetupGroups"

while IFS= read -r group; do
    testDeclarations+="void testTeardown_${group}(void); "
done <<< "$testTeardownGroups"

while IFS=$'\t' read -r group name; do
    setup="0"
    teardown="0"
    if $(echo $testSetupGroups | grep -q "$group"); then
        setup="testSetup_${group}"
    fi

    if $(echo $testTeardownGroups | grep -q "$group"); then
        teardown="testTeardown_${group}"
    fi

    testList+="{\"${group}\", \"${name}\", ${setup}, ${teardown}, test_${name}}, "
    testDeclarations+="void test_${name}(void); "
done < <(paste <(echo "$testGroup") <(echo "$testName"))

testList="${testList%, }};"

filename="${1##*/}"
testlistFile="testlists/${filename%.c}-list.c"
cp "template-test-list.c" "$testlistFile"

echo "ic $ignoredTestCount"

sed -i "s/^\\s*static\\s\+Test\\s\+testArray\\s*\[\]\\s*=.*;\\s*$/${testList}/g" "$testlistFile"
sed -i -E "s/^(\\s*void\\s+.*\(void\);)+/${testDeclarations}/g" "$testlistFile"
sed -i "s/\(^\\s*.ignoredTestCount\\s*=\\s*\)[0-9]\+\\s*,\?\\s*$/\1${ignoredTestCount}/g" "$testlistFile"
