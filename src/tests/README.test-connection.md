# test-connection - Network Transport Test Program

## Overview

`test-connection` is a simple test program that exercises the Mosh network transport layer directly, without requiring SSH or the full Mosh client/server infrastructure.

This is useful for:
- Testing the transport abstraction (Phase 1)
- Verifying UDP and TCP implementations work correctly
- Debugging network issues
- Understanding how the state synchronization protocol works

## Building

The test program is built as part of the normal test suite:

```bash
./autogen.sh
./configure
make
make check
```

The binary will be at: `src/tests/test-connection`

## Usage

### Start Server

```bash
./test-connection server [port]
```

If port is omitted or 0, a random port will be assigned.

The server will print:
- The port it's listening on
- The connection key (base64-encoded encryption key)
- The client command to use

Example:
```
$ ./test-connection server 60001
Starting test server on port 60001...
Server listening on 60001
Connection key: Ks8P3Z+L9N2xQ4V7Y1mH3g
Waiting for client connection...
Client command: ./test-connection client localhost 60001 Ks8P3Z+L9N2xQ4V7Y1mH3g
```

### Connect Client

```bash
./test-connection client <host> <port> <key>
```

Use the host, port, and key displayed by the server.

Example:
```
$ ./test-connection client localhost 60001 Ks8P3Z+L9N2xQ4V7Y1mH3g
Connecting to localhost:60001...
Connected!

Sending: Hello from client
  <- Server: Got message #1
Sending: Testing state sync
  <- Server: Got message #2
Sending: Message three
  <- Server: Got message #3
Sending: Final message
  <- Server: Got message #4

Shutting down...
Shutdown acknowledged.
Client exiting.
```

## What It Tests

### Phase 1 (Current)
- ✅ UDPConnection implementation
- ✅ ConnectionInterface abstraction
- ✅ Transport<> template with pointer-based connections
- ✅ State synchronization protocol
- ✅ Encryption/decryption
- ✅ RTT calculation
- ✅ Fragment assembly
- ✅ Graceful shutdown

### Phase 2 (Future)
- ⏳ TCPConnection implementation
- ⏳ TCP message framing
- ⏳ TCP reconnection logic
- ⏳ TCP vs UDP behavior comparison

## Testing TCP (Phase 2)

Once TCP implementation is complete, you'll be able to test both protocols:

```bash
# UDP (default)
./test-connection server udp 60001
./test-connection client udp localhost 60001 <key>

# TCP (future)
./test-connection server tcp 60001
./test-connection client tcp localhost 60001 <key>
```

## Architecture

The test program uses a simple `MockState` class that just holds a string message. This is much simpler than the full Terminal/UserStream states used by real Mosh, but exercises the same state synchronization protocol.

```
Client                    Server
------                    ------
MockState                 MockState
   |                         |
   v                         v
Transport<MockState>      Transport<MockState>
   |                         |
   v                         v
ConnectionInterface       ConnectionInterface
   |                         |
   v                         v
UDPConnection             UDPConnection
   |                         |
   +-------- Network ---------+
```

## Implementation Details

### MockState
- Holds a simple string message
- Implements `diff_from()` - returns new message if changed
- Implements `apply_string()` - sets message from diff
- Much simpler than real Terminal/UserStream states

### Message Flow
1. Client sets message via `set_message()`
2. Transport detects state change
3. Transport computes diff and fragments it
4. Packets encrypted and sent via UDPConnection
5. Server receives, decrypts, reassembles fragments
6. Server applies diff to its remote state
7. Server can read message via `get_message()`
8. Server sets response, cycle repeats

### Network Layer
- Uses actual Mosh Transport<> and UDPConnection classes
- Real encryption (AES-128-OCB)
- Real RTT calculation and congestion control
- Real packet fragmentation
- Real state synchronization protocol

## Troubleshooting

### Server won't start
- Check if port is already in use: `netstat -an | grep 60001`
- Try using port 0 to pick a random port

### Client can't connect
- Check firewall: `sudo iptables -L`
- Verify port is correct
- Verify key is exactly as printed by server

### Connection hangs
- Check that both sides are running
- Look for network issues: `ping <host>`
- Try increasing verbosity in code (see src/network/networktransport.h)

## Code Location

- Test program: `src/tests/test-connection.cc`
- Network layer: `src/network/`
- Connection interface: `src/network/connection_interface.h`
- UDP implementation: `src/network/network.{h,cc}`
- Transport: `src/network/networktransport*.h`

## Future Enhancements

Possible improvements:
- [ ] Add command-line verbosity flag
- [ ] Add interactive mode (read from stdin)
- [ ] Add performance benchmarking
- [ ] Add packet loss simulation
- [ ] Add latency measurement
- [ ] Add protocol selection (UDP/TCP)
- [ ] Add automated test suite
- [ ] Add stress testing mode
