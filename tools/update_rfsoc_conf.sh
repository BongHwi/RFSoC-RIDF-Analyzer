#!/bin/bash

# ==============================================================================
# Usage: ./set_fpga_smart.sh [Delay_ns] [Data_Count]
# Function: Automatically rounds input values to FPGA resolution (16ns, 8word) and checks range
# ==============================================================================

if [ $# -ne 2 ]; then
    echo "Usage: $0 [Delay(ns)] [Data Count(word)]"
    exit 1
fi

REQ_DELAY=$1
REQ_COUNT=$2

# --- 1. Delay Calculation (Rounding to 16ns unit) ---
# Logic: (Input + 8) / 16 -> Integer rounding effect
VAL_DL=$(( (REQ_DELAY + 8) / 16 ))
ACTUAL_DELAY=$(( VAL_DL * 16 ))

# --- 2. Count Calculation (Rounding to 8 word unit) ---
# Logic: First divide by 8 to get the number of chunks (Rounding)
# Formula is (Val + 1) * 8, so Chunk = Val + 1
CHUNK=$(( (REQ_COUNT + 4) / 8 ))

# Minimum value correction (If Chunk is 0, it must be at least 1 -> 8 words)
if [ "$CHUNK" -lt 1 ]; then CHUNK=1; fi

VAL_LEN=$(( CHUNK - 1 ))
ACTUAL_COUNT=$(( CHUNK * 8 ))

# --- 3. Range Check (0 ~ 1023) ---
if [ "$VAL_DL" -lt 0 ] || [ "$VAL_DL" -gt 1023 ]; then
    echo "Error: Delay range exceeded (Max: 16368 ns)"
    exit 1
fi

if [ "$VAL_LEN" -lt 0 ] || [ "$VAL_LEN" -gt 1023 ]; then
    echo "Error: Data Count range exceeded (Max: 8192 words)"
    exit 1
fi

# --- 4. Hexadecimal Conversion ---
HEX_DL=$(printf "%04x" "$VAL_DL")
HEX_LEN=$(printf "%04x" "$VAL_LEN")

# --- 5. Result Report (Important: For user verification) ---
echo "=================================================="
echo " FPGA Parameter Auto-Correction Result"
echo "=================================================="
echo " [Delay Setting]"
echo "  - Request : ${REQ_DELAY} ns"
echo "  - Actual  : ${ACTUAL_DELAY} ns (Hex: $HEX_DL)"
if [ "$REQ_DELAY" -ne "$ACTUAL_DELAY" ]; then
    echo "  * Note: Value adjusted to match resolution (16ns)."
fi
echo "--------------------------------------------------"
echo " [Count Setting]"
echo "  - Request : ${REQ_COUNT} words"
echo "  - Actual  : ${ACTUAL_COUNT} words (Hex: $HEX_LEN)"
if [ "$REQ_COUNT" -ne "$ACTUAL_COUNT" ]; then
    echo "  * Note: Value adjusted to match resolution (8 words)."
fi
echo "=================================================="
echo ""
echo " Apply to 5 servers (rf01~rf05)? (y/n)"
read -r CONFIRM

if [ "$CONFIRM" != "y" ]; then
    echo "Operation cancelled."
    exit 0
fi

# --- 6. Apply to Servers ---
SERVERS="rf01 rf02 rf03 rf04 rf05"
TARGET_FILE="bin/fpgainit.sh"

for host in $SERVERS; do
    printf ">> [%s] Applying... " "$host"
    ssh "$host" "sed -i -e 's/dl=..../dl=$HEX_DL/' -e 's/len=..../len=$HEX_LEN/' $TARGET_FILE && grep -E 'dl=|len=' $TARGET_FILE"
done

echo ""
echo "=== Configuration Complete ==="
