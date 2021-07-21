#!/bin/bash

# This script is intended to be used either by the CI or by a developer that
# wants to check that no unit tests have been broken to ensure there are no
# regressions. At the end, it reports how many tests were successful. However,
# using grep we can easily filter out everything except for those tests that
# time out. e.g. ./run_all.sh | grep -i timeout -B1

set -u -o pipefail

readonly SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
readonly GRAVITY_BIN=$SCRIPT_DIR/../../gravity
files=$(find $SCRIPT_DIR -iname "*.gravity" | grep -v disabled)
tests_total=$(echo "$files" | wc -l)
tests_success=0
tests_fail=0
tests_timeout=0
i=1

for test in $files; do
    echo "Testing $i/$tests_total - $test..."
    # Set 0.1s by default, but fallback to 10s in case of memory or recursion test
    timeout=0.1
    if [[ "$test" =~ "mem" || "$test" =~ "recursion" ]]; then
        timeout=10
    fi
    timeout $timeout "$GRAVITY_BIN" "$test"
    res=$?
    if [[ $res -eq 0 ]]; then
        tests_success=$(($tests_success+1))
        echo "Success!"
    elif [[ $res -eq 124 ]]; then
        echo "Timeout!"
        tests_timeout=$(($tests_timeout+1))
    else
        echo "Fail!"
        tests_fail=$(($tests_fail+1))
    fi
    i=$(($i+1))
done

echo "Tests run successfully: $tests_success/$tests_total. $tests_fail failed and $tests_timeout timed out"

if [[ $(($tests_fail+$tests_timeout)) -ne 0 ]]; then
    exit 1
fi
