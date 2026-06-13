#!/bin/bash

# Configuration
WEBSERV="./webserv"
TEST_DIR="tests/parser_tests"
JSONS_DIR="jsons"

# Counters
TOTAL=0
PASSED=0

run_test() {
    local file=$1
    local expected_fail=$2
    TOTAL=$((TOTAL + 1))
    
    echo -n "Testing $file... "
    $WEBSERV "$file" > /dev/null 2>&1
    local status=$?
    
    if [ $expected_fail -eq 1 ]; then
        if [ $status -ne 0 ]; then
            echo "PASSED (Rejected as expected)"
            PASSED=$((PASSED + 1))
        else
            echo "FAILED (Accepted invalid file)"
        fi
    else
        if [ $status -eq 0 ]; then
            echo "PASSED (Accepted valid file)"
            PASSED=$((PASSED + 1))
        else
            echo "FAILED (Rejected valid file)"
        fi
    fi
}

echo "=== Intensive Parser Tests ==="

# Test files that SHOULD fail
for f in $TEST_DIR/err_*.json; do
    run_test "$f" 1
done

# Also test files in jsons/ that are known to be bad
for f in $JSONS_DIR/json_*.json; do
    # All files in jsons/ should cause a failure in webserv either 
    # because of syntax or because they are not valid config objects.
    run_test "$f" 1
done

echo "=============================="
echo "Summary: $PASSED / $TOTAL tests passed"

if [ $PASSED -eq $TOTAL ]; then
    exit 0
else
    exit 1
fi
