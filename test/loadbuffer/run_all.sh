#!/bin/bash

# Regression tests for the JSON executable loader (gravity -x / gravity_vm_loadbuffer).
#
# Every .json file in this directory is a malformed JSON executable: loading it must
# be reported as a load error and must never crash the process. See issue #444, where
# {"x":{"type":"function"}} made gravity_vm_loadbuffer() call strlen() on the NULL
# identifier of a top level function.
#
# valid_roundtrip.gravity is the positive control: it is compiled and then executed
# through the very same loader, so the checks above cannot pass just because the
# loader started rejecting every input.
#
# json_bounds.c covers what the CLI cannot reach: gravity_vm_loadbuffer() accepts a
# buffer that is not NUL terminated, while the CLI always hands it one that is. Run
# `make jsontest` to build it, ideally with a sanitizer (see the file header).

set -u -o pipefail

readonly SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
readonly GRAVITY_BIN=$SCRIPT_DIR/../../gravity
readonly LOAD_ERROR="Error while loading compile file"

if [[ ! -x "$GRAVITY_BIN" ]]; then
    echo "gravity executable not found in $(dirname "$GRAVITY_BIN"), run make first"
    exit 1
fi

# Same portable timeout resolution used by test/unittest/run_all.sh:
# timeout (Linux/GNU coreutils) → gtimeout (macOS + brew install coreutils) →
# pure-bash fallback using a background kill watcher.
if command -v timeout &>/dev/null; then
    run_timeout() { timeout "$@"; }
elif command -v gtimeout &>/dev/null; then
    run_timeout() { gtimeout "$@"; }
else
    run_timeout() {
        local t=$1; shift
        "$@" &
        local pid=$!
        ( sleep "$t" && kill "$pid" 2>/dev/null ) &
        local watcher=$!
        wait "$pid" 2>/dev/null
        local rc=$?
        kill "$watcher" 2>/dev/null
        wait "$watcher" 2>/dev/null
        [[ $rc -eq 143 ]] && return 124
        return $rc
    }
fi

tests_success=0
tests_fail=0

report_success() {
    echo "Success!"
    tests_success=$(($tests_success+1))
}

report_fail() {
    echo "Fail! $1"
    tests_fail=$(($tests_fail+1))
}

# a malformed executable must be rejected, not crash
for test in "$SCRIPT_DIR"/*.json; do
    echo "Testing $(basename "$test")..."
    output=$(run_timeout 10 "$GRAVITY_BIN" -x "$test" 2>&1)
    res=$?

    if [[ $res -eq 124 ]]; then
        report_fail "timeout"
    elif [[ $res -ge 128 ]]; then
        # 128+n means the process was killed by signal n (139 = SIGSEGV)
        report_fail "killed by signal $(($res-128))"
    elif [[ "$output" != *"$LOAD_ERROR"* ]]; then
        report_fail "malformed input was not rejected: $output"
    else
        report_success
    fi
done

# a well formed executable must still load and run
readonly ROUNDTRIP_SRC=$SCRIPT_DIR/valid_roundtrip.gravity
readonly ROUNDTRIP_OUT=$(mktemp -d)/valid_roundtrip.json

echo "Testing $(basename "$ROUNDTRIP_SRC")..."
output=$(run_timeout 10 "$GRAVITY_BIN" -c "$ROUNDTRIP_SRC" -o "$ROUNDTRIP_OUT" 2>&1)
if [[ ! -f "$ROUNDTRIP_OUT" ]]; then
    report_fail "unable to compile $ROUNDTRIP_SRC: $output"
else
    output=$(run_timeout 10 "$GRAVITY_BIN" -x "$ROUNDTRIP_OUT" 2>&1)
    res=$?
    if [[ $res -ne 0 ]]; then
        report_fail "exit code $res"
    elif [[ "$output" == *"$LOAD_ERROR"* ]]; then
        report_fail "valid executable was rejected: $output"
    elif [[ "$output" != *"(INT) 0"* ]]; then
        report_fail "unexpected result: $output"
    else
        report_success
    fi
fi
rm -rf "$(dirname "$ROUNDTRIP_OUT")"

# scanner bounds tests, only if they have been built (make jsontest)
readonly JSONTEST_BIN=$SCRIPT_DIR/../../jsontest

echo "Testing json_bounds..."
if [[ ! -x "$JSONTEST_BIN" ]]; then
    echo "Skipped: run 'make jsontest' to build it"
else
    output=$(run_timeout 60 "$JSONTEST_BIN" 2>&1)
    res=$?

    if [[ $res -eq 124 ]]; then
        report_fail "timeout"
    elif [[ $res -ge 128 ]]; then
        report_fail "killed by signal $(($res-128))"
    elif [[ $res -ne 0 ]]; then
        report_fail "$output"
    else
        report_success
    fi
fi

tests_total=$(($tests_success+$tests_fail))
echo "Tests run successfully: $tests_success/$tests_total. $tests_fail failed"

[[ $tests_fail -ne 0 ]] && exit 1
exit 0
