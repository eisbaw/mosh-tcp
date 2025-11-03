#!/bin/bash
#
# test-network-layer.sh - Quick test of Mosh network transport layer
#
# This script demonstrates the test-connection program and verifies
# that Phase 1 transport abstraction works correctly.
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Paths
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BUILD_DIR="${SCRIPT_DIR}/../src/tests"
TEST_PROG="${BUILD_DIR}/test-connection"

echo "=== Mosh Network Transport Layer Test ==="
echo

# Check if program exists
if [ ! -x "$TEST_PROG" ]; then
    echo -e "${RED}Error: test-connection not found at $TEST_PROG${NC}"
    echo "Please build it first:"
    echo "  ./autogen.sh && ./configure && make"
    exit 1
fi

echo -e "${GREEN}✓ Found test-connection${NC}"
echo

# Create temp directory for test output
TMPDIR=$(mktemp -d)
SERVER_LOG="$TMPDIR/server.log"
CLIENT_LOG="$TMPDIR/client.log"
SERVER_INFO="$TMPDIR/server.info"

cleanup() {
    echo
    echo "Cleaning up..."
    if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -rf "$TMPDIR"
}
trap cleanup EXIT

echo "Starting test server..."

# Start server in background
"$TEST_PROG" server 0 > "$SERVER_LOG" 2>&1 &
SERVER_PID=$!

# Give server time to start
sleep 1

# Check if server is still running
if ! kill -0 "$SERVER_PID" 2>/dev/null; then
    echo -e "${RED}✗ Server failed to start${NC}"
    cat "$SERVER_LOG"
    exit 1
fi

# Extract connection info from server log
PORT=$(grep "Server listening on" "$SERVER_LOG" | awk '{print $4}')
KEY=$(grep "Connection key:" "$SERVER_LOG" | awk '{print $3}')

if [ -z "$PORT" ] || [ -z "$KEY" ]; then
    echo -e "${RED}✗ Failed to get server connection info${NC}"
    cat "$SERVER_LOG"
    exit 1
fi

echo -e "${GREEN}✓ Server started on port $PORT${NC}"
echo "  Connection key: $KEY"
echo

echo "Running client..."

# Run client
if "$TEST_PROG" client localhost "$PORT" "$KEY" > "$CLIENT_LOG" 2>&1; then
    echo -e "${GREEN}✓ Client completed successfully${NC}"
    echo

    # Show some output
    echo "=== Client Output ==="
    head -20 "$CLIENT_LOG"
    if [ $(wc -l < "$CLIENT_LOG") -gt 20 ]; then
        echo "..."
        tail -5 "$CLIENT_LOG"
    fi
    echo

    echo "=== Server Output ==="
    cat "$SERVER_LOG"
    echo

    echo -e "${GREEN}=== SUCCESS ===${NC}"
    echo "The network transport layer is working correctly!"
    echo
    echo "Tested:"
    echo "  ✓ UDPConnection implementation"
    echo "  ✓ ConnectionInterface abstraction"
    echo "  ✓ Transport<> template"
    echo "  ✓ State synchronization"
    echo "  ✓ Encryption/decryption"
    echo "  ✓ Graceful shutdown"

    exit 0
else
    echo -e "${RED}✗ Client failed${NC}"
    echo
    echo "=== Client Output ==="
    cat "$CLIENT_LOG"
    echo
    echo "=== Server Output ==="
    cat "$SERVER_LOG"
    exit 1
fi
