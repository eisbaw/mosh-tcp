# TCP Transport Support for Mosh

## Executive Summary

Mosh currently uses UDP exclusively for all traffic after SSH bootstrap. Some VPNs and restrictive networks block UDP, preventing Mosh from functioning. This plan outlines adding TCP transport as an alternative, maintaining Mosh's design philosophy of aggressive retries with short timeouts.

## Design Philosophy

- **Short timeouts**: Configurable but < 1 second by default (matching current UDP RTO max)
- **Infinite retries**: Never give up, like existing UDP behavior
- **Minimal changes**: Preserve existing UDP behavior and architecture
- **Protocol choice**: User-selectable at connection time via `--protocol=tcp|udp`
- **Backward compatible**: Default to UDP for existing behavior

## Current Architecture Analysis

### UDP Implementation (Baseline)

**Files:**
- `src/network/network.h/cc` - Connection class (UDP socket management)
- `src/network/networktransport.h` - Transport<> template (state sync layer)
- `src/network/transportsender.h/impl.h` - Retry/timeout logic
- `src/frontend/mosh-client.cc` - Client entry point
- `src/frontend/mosh-server.cc` - Server entry point
- `scripts/mosh.pl` - SSH bootstrap wrapper

**Current Timeout Behavior:**
```cpp
MIN_RTO = 50ms
MAX_RTO = 1000ms
RTO = ceil(SRTT + 4 * RTTVAR)  // RFC 2988
```

**Current Retry Behavior:**
- Retransmit unacknowledged data after RTO + ACK_DELAY (100ms)
- Send new data at adaptive rate: SRTT/2 (clamped 20-250ms)
- Send keepalive ACKs every 3 seconds if no data
- Port hopping every 10 seconds (client only, for NAT traversal)

**Key UDP Features:**
- Connectionless datagram model
- Port hopping for NAT/firewall traversal
- Application-level reliability via state synchronization
- Designed for high-latency, lossy networks

### Transport Layer Architecture

```
┌─────────────────────────────────────────┐
│  Application (mosh-client/mosh-server)  │
├─────────────────────────────────────────┤
│  Transport<MyState, RemoteState>        │  ← Protocol-agnostic
├─────────────────────────────────────────┤
│  TransportSender<MyState>               │  ← Protocol-agnostic
├─────────────────────────────────────────┤
│  Connection (CURRENT: UDP only)         │  ← NEEDS ABSTRACTION
├─────────────────────────────────────────┤
│  OS Network Stack                        │
└─────────────────────────────────────────┘
```

**Connection Class Interface (network.h:107-242):**
```cpp
class Connection {
  // Core interface (must be preserved):
  void send(const string& s);           // Send encrypted packet
  string recv(void);                    // Receive encrypted packet
  const vector<int> fds(void) const;    // Get socket FDs for select()
  uint64_t timeout(void) const;         // Get current RTO
  int get_MTU(void) const;              // Get application MTU

  // Connection state:
  string port(void) const;              // Local port
  string get_key(void) const;           // Encryption key
  bool get_has_remote_addr(void) const; // Have peer address?

  // RTT tracking:
  double get_SRTT(void) const;          // Smoothed RTT
  void set_last_roundtrip_success(...); // Update RTT
};
```

## TCP Transport Design

### High-Level Changes

1. **Abstract Transport Interface** - Extract common interface from Connection
2. **TCP Implementation** - New TCPConnection class
3. **UDP Refactoring** - Rename Connection → UDPConnection
4. **Protocol Selection** - Add runtime protocol choice
5. **Bootstrap Updates** - Pass protocol through SSH handshake
6. **Configuration** - New flags and environment variables

### Phase 1: Transport Abstraction

**Goal:** Make Transport<> work with any connection type.

**New Files:**
- `src/network/connection_interface.h` - Abstract base class for connections

**Modified Files:**
- `src/network/network.h` - Rename Connection → UDPConnection
- `src/network/network.cc` - Update implementation
- `src/network/networktransport.h` - Use ConnectionInterface*
- `src/network/networktransport-impl.h` - Update implementation

**Connection Interface:**
```cpp
// connection_interface.h
class ConnectionInterface {
public:
  virtual ~ConnectionInterface() {}

  // Core protocol-independent operations
  virtual void send(const std::string& s) = 0;
  virtual std::string recv(void) = 0;
  virtual const std::vector<int> fds(void) const = 0;
  virtual uint64_t timeout(void) const = 0;
  virtual int get_MTU(void) const = 0;

  // Connection info
  virtual std::string port(void) const = 0;
  virtual std::string get_key(void) const = 0;
  virtual bool get_has_remote_addr(void) const = 0;

  // RTT tracking (for Transport layer)
  virtual double get_SRTT(void) const = 0;
  virtual void set_last_roundtrip_success(uint64_t s) = 0;

  // Error reporting
  virtual std::string& get_send_error(void) = 0;

  // Address info (for diagnostics)
  virtual const Addr& get_remote_addr(void) const = 0;
  virtual socklen_t get_remote_addr_len(void) const = 0;
};
```

**Transport Changes:**
```cpp
// networktransport.h
template<class MyState, class RemoteState>
class Transport {
private:
  ConnectionInterface* connection;  // Was: Connection connection

public:
  // Factory constructors
  static Transport* create_udp(...);
  static Transport* create_tcp(...);
};
```

### Phase 2: TCP Connection Implementation

**New Files:**
- `src/network/tcpconnection.h`
- `src/network/tcpconnection.cc`

**TCP-Specific Design Decisions:**

#### Connection Model
- **Stream-based**: TCP provides ordered byte stream
- **Framing**: Need message framing since TCP has no packet boundaries
- **Stateful**: Connection-oriented (unlike UDP's connectionless model)

#### Message Framing
TCP is a byte stream, not message-oriented. We need framing:

```
Wire format: [4-byte length][encrypted payload]
```

Implementation:
```cpp
void TCPConnection::send(const string& payload) {
  uint32_t len = htonl(payload.size());
  write(fd, &len, 4);           // Send length prefix
  write(fd, payload.data(), payload.size());  // Send payload
}

string TCPConnection::recv() {
  uint32_t len;
  read(fd, &len, 4);            // Read length prefix
  len = ntohl(len);
  string payload(len, '\0');
  read(fd, &payload[0], len);   // Read payload
  return payload;
}
```

**Partial Read Handling:**
```cpp
// Need buffering for partial reads
class TCPConnection {
  string recv_buffer;           // Accumulate partial messages

  string recv() {
    while (true) {
      if (recv_buffer.size() >= 4) {
        uint32_t msg_len = parse_length(recv_buffer);
        if (recv_buffer.size() >= 4 + msg_len) {
          // Complete message available
          string msg = extract_message(recv_buffer);
          return msg;
        }
      }
      // Need more data
      char buf[4096];
      ssize_t n = read(fd, buf, sizeof(buf));
      if (n <= 0) handle_error();
      recv_buffer.append(buf, n);
    }
  }
};
```

#### Timeout Configuration

**TCP Requirements:**
- Short connect timeout (< 1 second)
- Short read/write timeouts (< 1 second)
- Quick failure detection

**Implementation via setsockopt():**
```cpp
void TCPConnection::setup() {
  // Connection timeout via non-blocking connect + select
  fcntl(fd, F_SETFL, O_NONBLOCK);
  connect(fd, addr, len);  // Non-blocking
  select(fd, timeout_ms);  // Wait with timeout

  // Read/write timeouts
  struct timeval tv = { .tv_sec = 0, .tv_usec = 500000 };  // 500ms default
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

  // TCP tuning for low latency
  int flag = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));  // Disable Nagle

  // Keepalive for connection monitoring
  setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag));

  // Aggressive keepalive timing
  int keepidle = 10;   // Start after 10s idle
  int keepintvl = 3;   // Probe every 3s
  int keepcnt = 3;     // 3 probes before failure
  setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
  setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
  setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));
}
```

**Configurable Timeouts:**
```cpp
// tcpconnection.h
class TCPConnection : public ConnectionInterface {
  static const uint64_t DEFAULT_TCP_TIMEOUT = 500;  // ms
  static const uint64_t MIN_TCP_TIMEOUT = 100;      // ms
  static const uint64_t MAX_TCP_TIMEOUT = 1000;     // ms

  uint64_t tcp_timeout;  // Configurable

public:
  void set_timeout(uint64_t ms);
  uint64_t timeout() const override { return tcp_timeout; }
};
```

#### Connection Failure Handling

**Failure Detection:**
- `read()` returns 0 → Connection closed by peer
- `read()` returns -1 with ETIMEDOUT → Timeout
- `read()` returns -1 with ECONNRESET → Connection reset
- `write()` returns -1 with EPIPE → Broken pipe

**Reconnection Strategy:**
```cpp
string TCPConnection::recv() {
  try {
    return recv_one();
  } catch (ConnectionLost& e) {
    // Log error
    if (verbose) {
      fprintf(stderr, "TCP connection lost: %s\n", e.what());
    }

    // Attempt reconnection
    reconnect();

    // Return empty string to trigger retry at application level
    return "";
  }
}

void TCPConnection::reconnect() {
  close(fd);
  fd = -1;

  // Retry forever (like UDP)
  while (!connected) {
    try {
      fd = socket(family, SOCK_STREAM, 0);
      setup();  // Set timeouts
      connect_with_timeout(remote_addr, tcp_timeout);
      connected = true;
      if (verbose) {
        fprintf(stderr, "TCP reconnected\n");
      }
    } catch (...) {
      // Wait briefly before retry
      usleep(100000);  // 100ms
    }
  }
}
```

#### RTT Estimation

**TCP has kernel-level RTT tracking**, but we can't access it portably. Options:

1. **Application-level RTT** (like UDP): Measure round-trip via timestamp echo
2. **TCP_INFO socket option** (Linux-specific): Read kernel RTT via `getsockopt()`
3. **Fixed conservative value**: Use 500ms constant

**Recommended: Application-level RTT** (reuse existing logic)
- Keep existing timestamp echo protocol
- Calculate SRTT/RTTVAR same as UDP
- Allows consistent behavior across protocols

```cpp
double TCPConnection::get_SRTT() const {
  return SRTT;  // Calculated from timestamp echoes, like UDP
}

uint64_t TCPConnection::timeout() const {
  uint64_t RTO = lrint(ceil(SRTT + 4 * RTTVAR));
  if (RTO < MIN_TCP_TIMEOUT) RTO = MIN_TCP_TIMEOUT;
  if (RTO > MAX_TCP_TIMEOUT) RTO = MAX_TCP_TIMEOUT;
  return RTO;
}
```

#### No Port Hopping

**UDP rationale:** Work around NAT binding timeouts
**TCP:** Not needed - stateful connection maintained by kernel

```cpp
// TCPConnection: No hop_port() method
// Connection stays on same port for entire session
```

#### MTU Handling

**UDP:** MTU matters to avoid IP fragmentation
**TCP:** Kernel handles segmentation transparently via MSS

```cpp
int TCPConnection::get_MTU() const {
  // TCP can handle larger messages
  // Still fragment at application level for encryption/compression
  return 8192;  // Larger than UDP default (500-1280)
}
```

### Phase 3: Protocol Selection

**Command-line Interface:**

```bash
# Client (via mosh wrapper)
mosh --protocol=tcp user@host
mosh --protocol=udp user@host  # Default

# Server (invoked by SSH)
mosh-server new --protocol=tcp
```

**Environment Variables:**
```bash
MOSH_PROTOCOL=tcp    # For scripting
MOSH_TCP_TIMEOUT=500 # TCP timeout in ms (default 500)
```

**Implementation:**

```cpp
// mosh-client.cc
enum class Protocol { UDP, TCP };

Protocol parse_protocol(const char* str) {
  if (strcmp(str, "tcp") == 0) return Protocol::TCP;
  if (strcmp(str, "udp") == 0) return Protocol::UDP;
  throw runtime_error("Invalid protocol");
}

int main(int argc, char* argv[]) {
  Protocol protocol = Protocol::UDP;  // Default

  const char* proto_env = getenv("MOSH_PROTOCOL");
  if (proto_env) {
    protocol = parse_protocol(proto_env);
  }

  // Parse --protocol flag (overrides environment)
  // ...

  // Create appropriate transport
  if (protocol == Protocol::TCP) {
    network = new Transport<...>::create_tcp(...);
  } else {
    network = new Transport<...>::create_udp(...);
  }
}
```

```cpp
// mosh-server.cc
int main(int argc, char* argv[]) {
  Protocol protocol = Protocol::UDP;  // Default

  // Parse --protocol flag
  // ...

  ServerConnection* network;
  if (protocol == Protocol::TCP) {
    network = ServerConnection::create_tcp(...);
  } else {
    network = ServerConnection::create_udp(...);
  }

  // Print MOSH CONNECT with protocol info
  printf("MOSH CONNECT %s %s %s\n",
         protocol == TCP ? "tcp" : "udp",
         network->port().c_str(),
         network->get_key().c_str());
}
```

### Phase 4: Bootstrap Changes

**Current Bootstrap (mosh.pl):**
```
1. SSH to server: ssh user@host 'mosh-server new'
2. Server prints: MOSH CONNECT <port> <key>
3. Extract port and key from SSH output
4. Kill SSH
5. Launch mosh-client with MOSH_KEY env var
```

**Updated Bootstrap:**
```
1. SSH to server: ssh user@host 'mosh-server new --protocol=tcp'
2. Server prints: MOSH CONNECT tcp <port> <key>
3. Extract protocol, port, and key from SSH output
4. Kill SSH
5. Launch mosh-client with MOSH_KEY and MOSH_PROTOCOL env vars
```

**Modified mosh.pl:**
```perl
# Add --protocol option
my $protocol = "udp";  # Default
GetOptions(..., "protocol=s" => \$protocol);

# Pass to mosh-server
my $server_command = "mosh-server new";
$server_command .= " --protocol=$protocol" if $protocol ne "udp";

# Parse output
my ($proto, $port, $key);
if ($line =~ /^MOSH CONNECT (\w+) (\d+) (\S+)/) {
  ($proto, $port, $key) = ($1, $2, $3);
} elsif ($line =~ /^MOSH CONNECT (\d+) (\S+)/) {
  # Backward compatibility: no protocol = UDP
  ($proto, $port, $key) = ("udp", $1, $2);
}

# Set environment
$ENV{MOSH_KEY} = $key;
$ENV{MOSH_PROTOCOL} = $proto;

# Launch client
exec("mosh-client", $host, $port);
```

### Phase 5: Testing & Validation

**Unit Tests:**
- TCP connection establishment
- Message framing (partial reads)
- Timeout behavior
- Reconnection logic
- Error handling

**Integration Tests:**
- Full client-server session over TCP
- TCP vs UDP behavior comparison
- Network failure scenarios
- High-latency simulation
- Packet loss tolerance

**Performance Tests:**
- Latency comparison (UDP vs TCP)
- Throughput measurement
- CPU usage
- Memory usage

**Compatibility Tests:**
- Backward compatibility (UDP still works)
- Mixed deployments (old client, new server)
- Various network conditions

## Implementation Phases

### Phase 1: Foundation (Week 1)
- [ ] Create `ConnectionInterface` abstract base
- [ ] Refactor `Connection` → `UDPConnection`
- [ ] Update `Transport<>` to use `ConnectionInterface*`
- [ ] Ensure UDP still works (no behavior change)

### Phase 2: TCP Core (Week 2)
- [ ] Implement `TCPConnection` class
- [ ] Message framing (length prefix)
- [ ] Timeout configuration
- [ ] Connection management
- [ ] Error handling

### Phase 3: Connection Resilience (Week 2-3)
- [ ] Reconnection logic
- [ ] Failure detection
- [ ] RTT estimation
- [ ] Keepalive configuration

### Phase 4: Integration (Week 3)
- [ ] Add protocol selection to mosh-server
- [ ] Add protocol selection to mosh-client
- [ ] Update mosh.pl bootstrap script
- [ ] Environment variable support

### Phase 5: Testing (Week 4)
- [ ] Unit tests
- [ ] Integration tests
- [ ] Performance benchmarks
- [ ] Network simulation tests

### Phase 6: Documentation (Week 4)
- [ ] Update man pages
- [ ] Add protocol selection docs
- [ ] Update README with TCP support info
- [ ] Add troubleshooting guide

## Configuration Reference

### New Command-Line Options

**mosh wrapper:**
```
--protocol=tcp|udp       Protocol to use (default: udp)
--tcp-timeout=<ms>       TCP timeout in milliseconds (default: 500)
```

**mosh-server:**
```
--protocol=tcp|udp       Protocol to use (default: udp)
--tcp-timeout=<ms>       TCP timeout in milliseconds (default: 500)
```

**mosh-client:**
(Reads from environment variables set by wrapper)

### Environment Variables

```
MOSH_PROTOCOL=tcp|udp           Protocol selection
MOSH_TCP_TIMEOUT=<ms>          TCP timeout (100-1000)
```

### Default Values

```cpp
// TCP timeout bounds
MIN_TCP_TIMEOUT = 100ms
DEFAULT_TCP_TIMEOUT = 500ms
MAX_TCP_TIMEOUT = 1000ms

// Reconnection behavior
RECONNECT_RETRY_INTERVAL = 100ms  // Wait between reconnect attempts
RECONNECT_FOREVER = true          // Never give up (like UDP)

// TCP socket options
TCP_NODELAY = true                // Disable Nagle's algorithm
SO_KEEPALIVE = true               // Enable TCP keepalive
TCP_KEEPIDLE = 10s                // Start keepalive after 10s
TCP_KEEPINTVL = 3s                // Probe every 3s
TCP_KEEPCNT = 3                   // 3 failed probes = dead
```

## Key Design Decisions

### 1. Keep Application-Level State Synchronization

**Decision:** TCP transport still uses Mosh's state-synchronization protocol

**Rationale:**
- TCP provides reliability, but not idempotency
- Mosh's protocol allows jumping to latest state (important for responsiveness)
- Encryption/compression still needed
- Consistent behavior across protocols

**Alternative considered:** Use TCP as raw stream, send terminal updates directly
- ❌ Would require major redesign
- ❌ Loss of roaming capability (future feature)
- ❌ No compression benefits

### 2. Aggressive Timeouts

**Decision:** Default TCP timeout of 500ms (configurable 100-1000ms)

**Rationale:**
- Match UDP's aggressive behavior (50-1000ms RTO range)
- Quick failure detection
- Better user experience on flaky connections
- User can tune via `--tcp-timeout`

**Alternative considered:** Use standard TCP timeouts (seconds to minutes)
- ❌ Would feel sluggish on poor networks
- ❌ Not aligned with Mosh philosophy

### 3. Automatic Reconnection

**Decision:** Reconnect forever on connection loss, like UDP retry behavior

**Rationale:**
- Consistent with Mosh's resilience philosophy
- Works through transient network issues
- User stays in session during network changes

**Alternative considered:** Fail immediately on connection loss
- ❌ Would break during brief network interruptions
- ❌ Poor user experience

### 4. No Roaming Support (Initially)

**Decision:** TCP connections stay on same IP/port (no port hopping)

**Rationale:**
- TCP is connection-oriented, can't change endpoints
- Port hopping is UDP-specific workaround for NAT
- Future: Could add IP roaming via connection re-establishment

**Alternative considered:** Implement TCP-level roaming (MPTCP, connection migration)
- ⚠️ Complex, non-standard
- ⚠️ Defer to future work

### 5. Length-Prefixed Framing

**Decision:** Use 4-byte length prefix for message framing

**Rationale:**
- Simple, efficient
- Minimal overhead (4 bytes per message)
- Easy to implement correctly

**Alternative considered:** Delimiter-based framing
- ❌ Need escaping
- ❌ Less efficient

## Backward Compatibility

### Protocol Version

No protocol version bump needed:
- Transport protocol unchanged (state synchronization)
- Only underlying network layer changes (TCP vs UDP)
- Encryption/compression unchanged

### Interoperability

**Old client, new server:**
- ✅ Works - server defaults to UDP

**New client, old server:**
- ✅ Works - client defaults to UDP
- ⚠️ TCP flag fails gracefully (old server doesn't understand)

**Migration path:**
- Update servers first (support both protocols)
- Update clients gradually
- UDP remains default for compatibility

## Security Considerations

### Attack Surface

**TCP-specific risks:**
- ⚠️ TCP handshake (SYN flood, etc.) - standard TCP mitigations apply
- ⚠️ Connection state tracking - limit concurrent connections
- ✅ Encryption unchanged (AES-128-OCB)
- ✅ Authentication unchanged (shared key from SSH)

**No new vulnerabilities introduced:**
- Encryption layer same as UDP
- State synchronization protocol unchanged
- Still requires SSH for bootstrap (authentication)

### Firewall Considerations

**TCP is more likely to be allowed:**
- Many networks block UDP but allow TCP
- TCP looks more "legitimate" to simple firewalls
- Better for enterprise/VPN environments

**Stealth implications:**
- TCP traffic may be less suspicious than UDP
- Standard port can be used (e.g., 443 for HTTPS-like appearance)

## Performance Expectations

### Latency

**Expected TCP overhead:**
- ~5-10% higher latency than UDP (due to ACKs)
- Negligible on high-latency links (>50ms)
- More noticeable on LAN (<5ms)

**Mitigations:**
- TCP_NODELAY disables Nagle's algorithm
- Application-level batching (existing in Mosh)

### Throughput

**TCP advantages:**
- Better congestion control
- Faster on high-bandwidth, high-latency links
- No fragmentation issues

**UDP advantages:**
- Lower overhead per packet
- Better for low-bandwidth links

### CPU Usage

**TCP expected impact:**
- Slightly lower (kernel handles retransmission)
- Framing overhead is minimal

## Open Questions

### 1. Connection Persistence Across Network Changes

**Question:** Should TCP reconnect on IP change (roaming)?

**Options:**
- A) Keep connection on original IP (simple, may break on network change)
- B) Detect network change and reconnect to new IP (complex, requires coordination)
- C) Use MPTCP (multipath TCP) for seamless migration (very complex)

**Recommendation:** Start with (A), evaluate need for (B) based on user feedback

### 2. Fallback Behavior

**Question:** Should client auto-fallback from TCP to UDP on failure?

**Options:**
- A) Fail if selected protocol doesn't work (explicit)
- B) Try TCP, fallback to UDP on timeout (automatic)
- C) Try both in parallel, use whichever connects first (fastest)

**Recommendation:** Start with (A) - explicit is better. Add (B) as optional flag.

### 3. Server Port Range

**Question:** Should TCP use different port range than UDP?

**Options:**
- A) Same port range (60001-60999) - simple
- B) Different port range for TCP - easier to firewall
- C) Configurable port range per protocol

**Recommendation:** Start with (A) for simplicity

## Success Metrics

### Functional Goals

- ✅ TCP transport works in environments that block UDP
- ✅ Timeout behavior is aggressive (< 1 second)
- ✅ Reconnection works reliably
- ✅ UDP behavior unchanged (backward compatible)

### Performance Goals

- TCP latency within 10% of UDP on same network
- Successful operation through VPNs that block UDP
- Connection recovery within 1 second of network restoration

### User Experience Goals

- Simple protocol selection (`--protocol=tcp`)
- Works out of the box (sensible defaults)
- Clear error messages on connection issues

## Future Enhancements

### Short-term (Months)
- Automatic protocol detection (try UDP, fallback to TCP)
- Per-protocol configuration (timeouts, keepalive)
- Better diagnostics (show protocol in use)

### Medium-term (6-12 months)
- TCP connection roaming (reconnect on IP change)
- Hybrid mode (use both UDP and TCP simultaneously)
- QUIC support (UDP-based but looks like TCP)

### Long-term (1+ years)
- Multipath support (MPTCP)
- Protocol negotiation (client/server agree on best protocol)
- Automatic MTU discovery (for both TCP and UDP)

## References

### Existing Mosh Behavior
- UDP RTO: 50-1000ms (RFC 2988 calculation)
- Port hopping: Every 10 seconds
- Server timeout: 40 seconds without client traffic
- State synchronization: Diff-based protocol

### Standards
- RFC 793: TCP
- RFC 2988: TCP RTO computation
- RFC 4821: PMTUD (future)

### Similar Projects
- **Eternal Terminal:** Uses TCP instead of UDP
- **tmux over SSH:** Pure TCP, but no local prediction
- **Secure Shell (SSH):** TCP only, higher latency

## Appendix: File Changes Summary

### New Files
```
src/network/connection_interface.h    - Abstract connection interface
src/network/tcpconnection.h          - TCP implementation header
src/network/tcpconnection.cc         - TCP implementation
```

### Modified Files
```
src/network/network.h                - Rename to UDPConnection
src/network/network.cc               - Update implementation
src/network/networktransport.h       - Use ConnectionInterface*
src/network/networktransport-impl.h  - Update to use interface
src/frontend/mosh-client.cc          - Add protocol selection
src/frontend/mosh-server.cc          - Add protocol selection
scripts/mosh.pl                      - Parse/pass protocol
configure.ac                         - Version bump, feature flag
man/mosh.1                           - Document --protocol flag
man/mosh-server.1                    - Document --protocol flag
README.md                            - Add TCP support documentation
```

### Test Files (New)
```
src/tests/test_tcpconnection.cc      - TCP connection tests
src/tests/test_tcp_integration.cc    - End-to-end TCP tests
src/tests/test_protocol_selection.cc - Protocol switching tests
```

## Risks & Mitigations

### Risk: TCP Head-of-Line Blocking

**Issue:** TCP delivers bytes in order. Retransmission blocks all subsequent data.

**Impact:** Could increase latency on lossy networks.

**Mitigation:**
- Use aggressive timeouts to detect loss quickly
- Application-level state sync allows skipping old data
- Users can choose UDP if TCP is problematic

### Risk: Backward Compatibility Break

**Issue:** Changes to Connection class could break UDP.

**Impact:** Existing deployments stop working.

**Mitigation:**
- Phase 1 focused on abstraction without behavior change
- Comprehensive testing of UDP behavior
- Keep UDP as default

### Risk: Reconnection Storms

**Issue:** Many clients reconnecting after network outage.

**Impact:** Server overload.

**Mitigation:**
- Random jitter in reconnection timing
- Rate limiting on server
- Connection limits per IP

### Risk: Performance Regression

**Issue:** TCP slower than UDP in some scenarios.

**Impact:** Poor user experience.

**Mitigation:**
- Benchmark both protocols
- Tune TCP socket options (NODELAY, etc.)
- Allow user to choose based on network

## Conclusion

Adding TCP transport to Mosh will enable operation in UDP-restricted networks while maintaining Mosh's design philosophy of aggressive timeouts and infinite retries. The abstraction approach preserves the existing UDP behavior while cleanly adding TCP as an alternative.

Key benefits:
- ✅ Works through VPNs that block UDP
- ✅ Maintains Mosh's responsiveness (short timeouts)
- ✅ Backward compatible (UDP unchanged)
- ✅ Clean architecture (connection abstraction)

The implementation is straightforward with well-defined phases and clear success metrics.
