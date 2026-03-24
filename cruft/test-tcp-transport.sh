#!/bin/bash

# TCP Transport Comprehensive Test Suite
# Tests the TCP transport implementation for mosh

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counters
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_TOTAL=0

# Set locale for mosh
export LC_ALL=C.UTF-8
export LANG=C.UTF-8

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Binaries
MOSH_SERVER="$BUILD_DIR/src/frontend/mosh-server"
MOSH_CLIENT="$BUILD_DIR/src/frontend/mosh-client"

# Test output file
TEST_LOG="/tmp/mosh-tcp-test-$$.log"

# Cleanup function
cleanup() {
    # Kill any remaining mosh servers
    pkill -f "mosh-server" 2>/dev/null || true
    rm -f "$TEST_LOG"
}

trap cleanup EXIT

# Logging functions
log_test() {
    echo -e "${YELLOW}[TEST]${NC} $1"
    ((TESTS_TOTAL++))
}

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
    ((TESTS_PASSED++))
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    ((TESTS_FAILED++))
}

log_info() {
    echo -e "[INFO] $1"
}

# Test functions

test_tcp_server_startup() {
    log_test "TCP server startup"

    if timeout 2 "$MOSH_SERVER" new -P tcp -p 60201 -- bash 2>&1 | grep -q "MOSH CONNECT tcp 60201"; then
        log_pass "TCP server starts and prints correct MOSH CONNECT"
        return 0
    else
        log_fail "TCP server failed to start or incorrect output"
        return 1
    fi
}

test_udp_server_startup() {
    log_test "UDP server startup (regression)"

    if timeout 2 "$MOSH_SERVER" new -P udp -p 60202 -- bash 2>&1 | grep -q "MOSH CONNECT udp 60202"; then
        log_pass "UDP server starts correctly"
        return 0
    else
        log_fail "UDP server failed to start"
        return 1
    fi
}

test_default_protocol() {
    log_test "Default protocol (should be UDP)"

    if timeout 2 "$MOSH_SERVER" new -p 60203 -- bash 2>&1 | grep -q "MOSH CONNECT udp 60203"; then
        log_pass "Default protocol is UDP"
        return 0
    else
        log_fail "Default protocol is not UDP"
        return 1
    fi
}

test_tcp_timeout_option() {
    log_test "TCP timeout option"

    if timeout 2 "$MOSH_SERVER" new -P tcp -T 300 -p 60204 -- bash 2>&1 | grep -q "MOSH CONNECT tcp 60204"; then
        log_pass "TCP timeout option accepted"
        return 0
    else
        log_fail "TCP timeout option failed"
        return 1
    fi
}

test_invalid_protocol() {
    log_test "Invalid protocol rejection"

    if "$MOSH_SERVER" new -P invalid -p 60205 -- bash 2>&1 | grep -q "Invalid protocol"; then
        log_pass "Invalid protocol correctly rejected"
        return 0
    else
        log_fail "Invalid protocol not rejected"
        return 1
    fi
}

test_tcp_server_child_process() {
    log_test "TCP server creates child process"

    OUTPUT=$(timeout 3 "$MOSH_SERVER" new -P tcp -p 60206 -- sleep 10 2>&1)

    if echo "$OUTPUT" | grep -q "MOSH CONNECT tcp 60206"; then
        # Server started successfully
        sleep 2
        # Check if the sleep process (child of server) is running
        if ps aux | grep "sleep 10" | grep -v grep >/dev/null; then
            log_pass "TCP server creates and maintains child process"
            pkill -f "sleep 10" 2>/dev/null || true
            return 0
        fi
    fi

    log_fail "TCP server failed to create child process"
    pkill -f "sleep 10" 2>/dev/null || true
    return 1
}

test_udp_server_child_process() {
    log_test "UDP server creates child process (regression)"

    OUTPUT=$(timeout 3 "$MOSH_SERVER" new -P udp -p 60207 -- sleep 10 2>&1)

    if echo "$OUTPUT" | grep -q "MOSH CONNECT udp 60207"; then
        # Server started successfully
        sleep 2
        # Check if the sleep process (child of server) is running
        if ps aux | grep "sleep 10" | grep -v grep >/dev/null; then
            log_pass "UDP server creates and maintains child process"
            pkill -f "sleep 10" 2>/dev/null || true
            return 0
        fi
    fi

    log_fail "UDP server failed to create child process"
    pkill -f "sleep 10" 2>/dev/null || true
    return 1
}

test_server_no_crash() {
    log_test "Server doesn't crash on startup"

    # Start both TCP and UDP servers
    timeout 3 "$MOSH_SERVER" new -P tcp -p 60208 -- sleep 5 > /tmp/tcp-test.log 2>&1 &
    TCP_PID=$!

    timeout 3 "$MOSH_SERVER" new -P udp -p 60209 -- sleep 5 > /tmp/udp-test.log 2>&1 &
    UDP_PID=$!

    sleep 2

    # Check if child processes are running (proves server didn't crash)
    TCP_CHILD=$(ps aux | grep "sleep 5" | grep -v grep | wc -l)

    # Wait for background jobs
    wait $TCP_PID 2>/dev/null || true
    wait $UDP_PID 2>/dev/null || true

    # Check logs for successful startup
    TCP_OK=false
    UDP_OK=false

    if grep -q "MOSH CONNECT tcp" /tmp/tcp-test.log && [ $TCP_CHILD -ge 1 ]; then
        TCP_OK=true
    fi

    if grep -q "MOSH CONNECT udp" /tmp/udp-test.log; then
        UDP_OK=true
    fi

    pkill -f "sleep 5" 2>/dev/null || true
    rm -f /tmp/tcp-test.log /tmp/udp-test.log

    if $TCP_OK && $UDP_OK; then
        log_pass "Servers start without crashing"
        return 0
    else
        log_fail "Server startup issue (TCP: $TCP_OK, UDP: $UDP_OK)"
        return 1
    fi
}

test_tcp_key_generation() {
    log_test "TCP server generates encryption key"

    OUTPUT=$(timeout 2 "$MOSH_SERVER" new -P tcp -p 60211 -- bash 2>&1)

    if echo "$OUTPUT" | grep -q "MOSH CONNECT tcp 60211 [A-Za-z0-9/+]\{22\}"; then
        log_pass "TCP server generates valid encryption key"
        return 0
    else
        log_fail "TCP server key generation failed"
        return 1
    fi
}

test_protocol_in_output() {
    log_test "Protocol correctly included in MOSH CONNECT output"

    TCP_OUTPUT=$(timeout 2 "$MOSH_SERVER" new -P tcp -p 60212 -- bash 2>&1)
    UDP_OUTPUT=$(timeout 2 "$MOSH_SERVER" new -P udp -p 60213 -- bash 2>&1)

    TCP_OK=false
    UDP_OK=false

    if echo "$TCP_OUTPUT" | grep -q "^MOSH CONNECT tcp"; then
        TCP_OK=true
    fi

    if echo "$UDP_OUTPUT" | grep -q "^MOSH CONNECT udp"; then
        UDP_OK=true
    fi

    if $TCP_OK && $UDP_OK; then
        log_pass "Protocol correctly included in output"
        return 0
    else
        log_fail "Protocol output test failed (TCP: $TCP_OK, UDP: $UDP_OK)"
        return 1
    fi
}

# Main test execution
main() {
    echo "======================================"
    echo "Mosh TCP Transport Test Suite"
    echo "======================================"
    echo ""

    log_info "Testing binaries:"
    log_info "  mosh-server: $MOSH_SERVER"
    log_info "  mosh-client: $MOSH_CLIENT"
    echo ""

    # Check if binaries exist
    if [ ! -x "$MOSH_SERVER" ]; then
        echo -e "${RED}ERROR:${NC} mosh-server not found at $MOSH_SERVER"
        exit 1
    fi

    if [ ! -x "$MOSH_CLIENT" ]; then
        echo -e "${RED}ERROR:${NC} mosh-client not found at $MOSH_CLIENT"
        exit 1
    fi

    echo "Running tests..."
    echo ""

    # Run tests
    test_tcp_server_startup || true
    test_udp_server_startup || true
    test_default_protocol || true
    test_tcp_timeout_option || true
    test_invalid_protocol || true
    test_tcp_server_child_process || true
    test_udp_server_child_process || true
    test_server_no_crash || true
    test_tcp_key_generation || true
    test_protocol_in_output || true

    # Summary
    echo ""
    echo "======================================"
    echo "Test Summary"
    echo "======================================"
    echo -e "Total tests:  $TESTS_TOTAL"
    echo -e "${GREEN}Passed:${NC}       $TESTS_PASSED"
    echo -e "${RED}Failed:${NC}       $TESTS_FAILED"
    echo ""

    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "${GREEN}All tests passed!${NC}"
        exit 0
    else
        echo -e "${RED}Some tests failed.${NC}"
        exit 1
    fi
}

main "$@"
