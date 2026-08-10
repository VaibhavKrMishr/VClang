#!/bin/bash

# Simple test runner for VClang

echo "======================================"
echo "    Running VClang Test Suite..."
echo "======================================"

PASSED=0
FAILED=0

for test_file in tests/*.vclang; do
    echo "Running $test_file..."
    
    # Run the interpreter and capture output/errors
    output=$(./vclang "$test_file" 2>&1)
    exit_code=$?
    
    if [ $exit_code -eq 0 ]; then
        echo -e "[\033[32mPASS\033[0m] $test_file"
        ((PASSED++))
    else
        echo -e "[\033[31mFAIL\033[0m] $test_file"
        echo "Output:"
        echo "$output"
        ((FAILED++))
    fi
done

echo "======================================"
echo "Tests Passed: $PASSED"
echo "Tests Failed: $FAILED"
echo "======================================"

if [ $FAILED -ne 0 ]; then
    exit 1
fi
exit 0
