#!/bin/bash

# Simple test runner for VClang

echo "======================================"
echo "    Running VClang Test Suite..."
echo "======================================"

PASSED=0
FAILED=0

for test_file in tests/*.vclang; do
    if [ "$test_file" == "tests/test_billion_loop.vclang" ]; then
        echo -e "[\033[33mSKIP\033[0m] $test_file (1B iterations)"
        continue
    fi

    echo "Running $test_file..."
    
    # Run the interpreter and capture output/errors
    output=$(./vclang "$test_file" 2>&1)
    exit_code=$?
    
    if [[ "$test_file" == *"_error_"* ]]; then
        # Negative test expects Error output
        if echo "$output" | grep -q "Error"; then
            echo -e "[\033[32mPASS (Expected Error)\033[0m] $test_file"
            ((PASSED++))
        else
            echo -e "[\033[31mFAIL (Expected Error but Succeeded)\033[0m] $test_file"
            echo "Output:"
            echo "$output"
            ((FAILED++))
        fi
    else
        # Positive test expects clean pass without Error
        if [ $exit_code -eq 0 ] && ! echo "$output" | grep -q "Error"; then
            echo -e "[\033[32mPASS\033[0m] $test_file"
            ((PASSED++))
        else
            echo -e "[\033[31mFAIL\033[0m] $test_file"
            echo "Output:"
            echo "$output"
            ((FAILED++))
        fi
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
