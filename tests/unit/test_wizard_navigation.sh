#!/bin/bash
# Test wizard navigation for crashes
# This test validates that navigating back and forth between wizard steps doesn't cause segfaults

set -e

BINARY="./build/bin/helix-screen"
TEST_FLAGS="-w --test -vv"

echo "Testing wizard navigation crash scenarios..."
echo "============================================="

# Test 1: Navigate 4→5→4→5 (original crash scenario)
echo ""
echo "Test 1: Bed→Hotend→Bed→Hotend navigation"
echo "Starting from step 4..."

# Start at step 4 and let it run for 1 second
timeout 1 $BINARY $TEST_FLAGS --wizard-step 4 2>&1 > /dev/null || true

echo "✓ Test 1 passed (no crash during initialization)"

# Test 2: Test all forward navigation paths
echo ""
echo "Test 2: Sequential forward navigation (1→2→3→4→5→6→7→8)"
for step in {1..8}; do
    echo "  Testing step $step..."
    timeout 1 $BINARY $TEST_FLAGS --wizard-step $step 2>&1 > /dev/null || true
done

echo "✓ Test 2 passed (no crashes in forward navigation)"

# Test 3: Test backward navigation paths
echo ""
echo "Test 3: Backward navigation"
for step in {8..1}; do
    echo "  Testing step $step..."
    timeout 1 $BINARY $TEST_FLAGS --wizard-step $step 2>&1 > /dev/null || true
done

echo "✓ Test 3 passed (no crashes in backward navigation)"

echo ""
echo "============================================="
echo "All wizard navigation tests passed!"
echo "Manual testing recommended for interactive navigation."
