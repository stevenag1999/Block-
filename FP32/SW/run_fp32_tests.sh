#!/bin/bash
# Test script for FP32 Accelerator

set -e

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

EXECUTABLE="./fp32_host"
N_BLOCKS=${N_BLOCKS:-6}

echo "========================================"
echo "FP32 Accelerator Test Suite"
echo "========================================"
echo ""

# Test operations
for op in 2 3 4 5 6; do
    echo "========================================"
    echo -e "${BLUE}Testing ${FP32_OP_NAMES[$op-2]} with $N_BLOCKS blocks${NC}"
    echo "========================================"
    $EXECUTABLE $op $N_BLOCKS
    echo ""
done

echo "========================================"
echo -e "${GREEN}✓ All FP32 tests completed!${NC}"
echo "========================================"
