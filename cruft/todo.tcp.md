# TCP Transport Implementation TODO

This document breaks down the TCP transport implementation into fine-grained, actionable tasks.

## Legend
- `[ ]` - Not started
- `[~]` - In progress
- `[x]` - Completed
- `[!]` - Blocked

---

## Phase 1: Transport Abstraction Layer (Foundation)

### 1.1 Create ConnectionInterface Base Class
- [ ] Create `src/network/connection_interface.h`
- [ ] Add copyright/license header
- [ ] Define `ConnectionInterface` abstract base class
- [ ] Add virtual destructor
- [ ] Declare `virtual void send(const std::string& s) = 0`
- [ ] Declare `virtual std::string recv(void) = 0`
- [ ] Declare `virtual const std::vector<int> fds(void) const = 0`
- [ ] Declare `virtual uint64_t timeout(void) const = 0`
- [ ] Declare `virtual int get_MTU(void) const = 0`
- [ ] Declare `virtual std::string port(void) const = 0`
- [ ] Declare `virtual std::string get_key(void) const = 0`
- [ ] Declare `virtual bool get_has_remote_addr(void) const = 0`
- [ ] Declare `virtual double get_SRTT(void) const = 0`
- [ ] Declare `virtual void set_last_roundtrip_success(uint64_t) = 0`
- [ ] Declare `virtual std::string& get_send_error(void) = 0`
- [ ] Declare `virtual const Addr& get_remote_addr(void) const = 0`
- [ ] Declare `virtual socklen_t get_remote_addr_len(void) const = 0`
- [ ] Add inline documentation for each method
- [ ] Add include guards

### 1.2 Refactor Connection → UDPConnection (Header)
- [ ] Open `src/network/network.h`
- [ ] Add `#include "connection_interface.h"`
- [ ] Rename `class Connection` to `class UDPConnection`
- [ ] Make `UDPConnection` inherit from `ConnectionInterface`
- [ ] Add `override` keyword to all virtual methods
- [ ] Update constructor documentation
- [ ] Add comment explaining UDP-specific features (port hopping)
- [ ] Verify no other changes to public interface needed
- [ ] Update header guard if needed

### 1.3 Refactor Connection → UDPConnection (Implementation)
- [ ] Open `src/network/network.cc`
- [ ] Replace all `Connection::` with `UDPConnection::`
- [ ] Update constructor names
- [ ] Update method definitions
- [ ] Verify all method signatures match interface
- [ ] Ensure no functionality changes (pure refactor)
- [ ] Check for any static Connection references to update

### 1.4 Update Transport Template (Header)
- [ ] Open `src/network/networktransport.h`
- [ ] Change `Connection connection;` to `ConnectionInterface* connection;`
- [ ] Update constructor signatures to accept protocol parameter
- [ ] Add destructor to delete connection pointer
- [ ] Update all `connection.method()` to `connection->method()`
- [ ] Add static factory methods:
  - [ ] `static Transport* create_udp(...)`
  - [ ] `static Transport* create_tcp(...)`
- [ ] Update documentation
- [ ] Add protocol enum or use string for now

### 1.5 Update Transport Template (Implementation)
- [ ] Open `src/network/networktransport-impl.h`
- [ ] Update constructors to allocate UDPConnection dynamically
- [ ] Add `delete connection` in destructor
- [ ] Replace all `connection.method()` with `connection->method()`
- [ ] Implement factory methods (TCP returns nullptr for now)
- [ ] Verify all connection accesses updated
- [ ] Check error handling paths

### 1.6 Update TransportSender (Header)
- [ ] Open `src/network/transportsender.h`
- [ ] Change `Connection* connection;` type if needed
- [ ] Update to use `ConnectionInterface*`
- [ ] Verify all method signatures compatible
- [ ] Update documentation

### 1.7 Update TransportSender (Implementation)
- [ ] Open `src/network/transportsender-impl.h`
- [ ] Update all connection accesses to use interface
- [ ] Verify no UDP-specific assumptions
- [ ] Test that it works with abstract interface

### 1.8 Update Build System
- [ ] Open `Makefile.am` or equivalent
- [ ] Add `connection_interface.h` to sources
- [ ] Update dependencies for network library
- [ ] Ensure header gets installed if needed

### 1.9 Update Client Code
- [ ] Open `src/frontend/mosh-client.cc`
- [ ] Update includes if needed
- [ ] Change Connection references to UDPConnection
- [ ] Verify compilation

### 1.10 Update Server Code
- [ ] Open `src/frontend/mosh-server.cc`
- [ ] Update includes if needed
- [ ] Change Connection references to UDPConnection
- [ ] Verify compilation

### 1.11 Update STMClient
- [ ] Open `src/frontend/stmclient.h`
- [ ] Update Connection references
- [ ] Update Transport template usage
- [ ] Open `src/frontend/stmclient.cc`
- [ ] Update implementation

### 1.12 Find and Update All Other Connection References
- [ ] Use grep to find all "Connection" references
- [ ] Update `#include` statements
- [ ] Update type declarations
- [ ] Update variable names if needed for clarity
- [ ] Check comments and documentation

### 1.13 Phase 1 Testing
- [ ] Compile codebase: `./autogen.sh && ./configure && make`
- [ ] Fix any compilation errors
- [ ] Run existing unit tests
- [ ] Run integration tests
- [ ] Verify UDP behavior unchanged
- [ ] Test client-server connection
- [ ] Test with various network conditions
- [ ] Check for memory leaks with valgrind

### 1.14 Phase 1 Cleanup
- [ ] Run clang-format on modified files
- [ ] Fix any warnings
- [ ] Update copyright years if needed
- [ ] Review all changes
- [ ] Create git commit for Phase 1

---

## Phase 2: TCP Connection Core Implementation

### 2.1 Create TCPConnection Header File
- [ ] Create `src/network/tcpconnection.h`
- [ ] Add copyright/license header
- [ ] Add include guards
- [ ] Include necessary headers:
  - [ ] `<string>`
  - [ ] `<vector>`
  - [ ] `<sys/socket.h>`
  - [ ] `<netinet/in.h>`
  - [ ] `<netinet/tcp.h>`
  - [ ] `"connection_interface.h"`
  - [ ] `"src/crypto/crypto.h"`

### 2.2 Define TCPConnection Class Structure
- [ ] Declare `class TCPConnection : public ConnectionInterface`
- [ ] Add public constructor: `TCPConnection(const char* desired_ip, const char* desired_port)` (server)
- [ ] Add public constructor: `TCPConnection(const char* key_str, const char* ip, const char* port)` (client)
- [ ] Add destructor: `~TCPConnection()`
- [ ] Add copy constructor (delete or implement)
- [ ] Add assignment operator (delete or implement)

### 2.3 Define TCPConnection Constants
- [ ] `static const uint64_t DEFAULT_TCP_TIMEOUT = 500;` // ms
- [ ] `static const uint64_t MIN_TCP_TIMEOUT = 100;` // ms
- [ ] `static const uint64_t MAX_TCP_TIMEOUT = 1000;` // ms
- [ ] `static const int DEFAULT_TCP_MTU = 8192;` // Larger than UDP
- [ ] `static const int CONNECT_TIMEOUT = 1000;` // ms
- [ ] `static const int MAX_MESSAGE_SIZE = 65536;` // Max frame size
- [ ] `static const int RECONNECT_DELAY = 100;` // ms between reconnect attempts

### 2.4 Define TCPConnection Private Members
- [ ] `int fd;` // Socket file descriptor
- [ ] `bool server;` // Server or client mode
- [ ] `bool connected;` // Connection state
- [ ] `int MTU;` // Application MTU
- [ ] `Base64Key key;` // Encryption key
- [ ] `Session session;` // Crypto session
- [ ] `Addr remote_addr;` // Remote endpoint
- [ ] `socklen_t remote_addr_len;` // Address length
- [ ] `bool has_remote_addr;` // Have remote address?
- [ ] `std::string recv_buffer;` // Partial message buffer
- [ ] `uint64_t tcp_timeout;` // Current timeout value
- [ ] `Direction direction;` // TO_SERVER or TO_CLIENT
- [ ] `uint16_t saved_timestamp;` // For RTT calculation
- [ ] `uint64_t saved_timestamp_received_at;` // RTT tracking
- [ ] `uint64_t expected_receiver_seq;` // Sequence tracking
- [ ] `bool RTT_hit;` // Have RTT measurement?
- [ ] `double SRTT;` // Smoothed RTT
- [ ] `double RTTVAR;` // RTT variance
- [ ] `uint64_t last_heard;` // Last packet time
- [ ] `std::string send_error;` // Error message
- [ ] `std::string bind_ip;` // Server bind address
- [ ] `std::string bind_port;` // Server bind port
- [ ] `unsigned int verbose;` // Verbosity level

### 2.5 Define TCPConnection Private Helper Methods
- [ ] `void setup();` // Configure socket options
- [ ] `void setup_socket_options();` // Set TCP_NODELAY, timeouts, etc.
- [ ] `void connect_with_timeout(const Addr& addr, uint64_t timeout_ms);`
- [ ] `void bind_and_listen(const char* ip, const char* port);`
- [ ] `void accept_connection();` // Server accepts client
- [ ] `void reconnect();` // Reconnect after failure
- [ ] `void close_connection();` // Clean shutdown
- [ ] `bool is_connected() const;` // Check connection state
- [ ] `ssize_t read_fully(void* buf, size_t len);` // Read exact bytes
- [ ] `ssize_t write_fully(const void* buf, size_t len);` // Write exact bytes
- [ ] `std::string recv_one();` // Receive one message
- [ ] `Packet new_packet(const std::string& payload);` // Create packet
- [ ] `void update_rtt(uint16_t timestamp_reply);` // Update RTT estimates

### 2.6 Implement ConnectionInterface Methods (Declarations)
- [ ] `void send(const std::string& s) override;`
- [ ] `std::string recv() override;`
- [ ] `const std::vector<int> fds() const override;`
- [ ] `uint64_t timeout() const override;`
- [ ] `int get_MTU() const override;`
- [ ] `std::string port() const override;`
- [ ] `std::string get_key() const override;`
- [ ] `bool get_has_remote_addr() const override;`
- [ ] `double get_SRTT() const override;`
- [ ] `void set_last_roundtrip_success(uint64_t s) override;`
- [ ] `std::string& get_send_error() override;`
- [ ] `const Addr& get_remote_addr() const override;`
- [ ] `socklen_t get_remote_addr_len() const override;`

### 2.7 Add TCPConnection Public Helper Methods
- [ ] `void set_timeout(uint64_t ms);` // Configure timeout
- [ ] `void set_verbose(unsigned int v);` // Set verbosity

### 2.8 Create TCPConnection Implementation File
- [ ] Create `src/network/tcpconnection.cc`
- [ ] Add copyright/license header
- [ ] Include `tcpconnection.h`
- [ ] Include system headers:
  - [ ] `<unistd.h>`
  - [ ] `<fcntl.h>`
  - [ ] `<errno.h>`
  - [ ] `<arpa/inet.h>`
  - [ ] `<netdb.h>`
  - [ ] `<poll.h>`

### 2.9 Implement TCPConnection Constructors

#### 2.9.1 Server Constructor
- [ ] Implement `TCPConnection(const char* desired_ip, const char* desired_port)`
- [ ] Initialize member variables
- [ ] Set `server = true`
- [ ] Set `direction = TO_CLIENT`
- [ ] Generate random encryption key
- [ ] Initialize crypto session
- [ ] Call `bind_and_listen(desired_ip, desired_port)`
- [ ] Set `connected = false` (wait for client)
- [ ] Set `has_remote_addr = false`

#### 2.9.2 Client Constructor
- [ ] Implement `TCPConnection(const char* key_str, const char* ip, const char* port)`
- [ ] Initialize member variables
- [ ] Set `server = false`
- [ ] Set `direction = TO_SERVER`
- [ ] Parse and set encryption key from key_str
- [ ] Initialize crypto session with key
- [ ] Resolve hostname to address
- [ ] Store remote_addr
- [ ] Call `connect_with_timeout(remote_addr, CONNECT_TIMEOUT)`
- [ ] Set `connected = true` on success
- [ ] Set `has_remote_addr = true`

### 2.10 Implement TCPConnection Destructor
- [ ] Close socket if open
- [ ] Clean up any allocated resources
- [ ] Clear sensitive data (keys)

### 2.11 Implement Socket Setup Methods

#### 2.11.1 Implement setup()
- [ ] Call `setup_socket_options()`
- [ ] Set `tcp_timeout` to DEFAULT_TCP_TIMEOUT
- [ ] Initialize RTT variables
- [ ] Initialize timestamps

#### 2.11.2 Implement setup_socket_options()
- [ ] Set socket to non-blocking: `fcntl(fd, F_SETFL, O_NONBLOCK)`
- [ ] Enable TCP_NODELAY (disable Nagle):
  - [ ] `int flag = 1;`
  - [ ] `setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag))`
- [ ] Set SO_KEEPALIVE:
  - [ ] `setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag))`
- [ ] Set TCP_KEEPIDLE (Linux-specific):
  - [ ] `int keepidle = 10;`
  - [ ] `setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle))`
- [ ] Set TCP_KEEPINTVL:
  - [ ] `int keepintvl = 3;`
  - [ ] `setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl))`
- [ ] Set TCP_KEEPCNT:
  - [ ] `int keepcnt = 3;`
  - [ ] `setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt))`
- [ ] Set SO_RCVTIMEO (receive timeout):
  - [ ] `struct timeval tv = {0, tcp_timeout * 1000};`
  - [ ] `setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))`
- [ ] Set SO_SNDTIMEO (send timeout):
  - [ ] `setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv))`
- [ ] Handle platform differences (macOS vs Linux)
- [ ] Add error checking for each setsockopt call

### 2.12 Implement Server Socket Methods

#### 2.12.1 Implement bind_and_listen()
- [ ] Create socket: `fd = socket(AF_INET, SOCK_STREAM, 0)`
- [ ] Handle IPv6: check desired_ip for IPv6 format
- [ ] Set SO_REUSEADDR:
  - [ ] `int flag = 1;`
  - [ ] `setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag))`
- [ ] Parse port number
- [ ] Setup sockaddr structure (IPv4 or IPv6)
- [ ] Bind socket: `bind(fd, addr, addrlen)`
- [ ] Handle EADDRINUSE error (port in use)
- [ ] Listen: `listen(fd, 1)` (backlog = 1)
- [ ] Store bind_ip and bind_port for later
- [ ] Call setup()
- [ ] Throw NetworkException on error

#### 2.12.2 Implement accept_connection()
- [ ] Only call if server mode
- [ ] Call `accept()` on listening socket
- [ ] Store client address in remote_addr
- [ ] Set remote_addr_len
- [ ] Close listening socket
- [ ] Update fd to connected socket
- [ ] Set `connected = true`
- [ ] Set `has_remote_addr = true`
- [ ] Call setup_socket_options() on new socket
- [ ] Determine MTU based on address family
- [ ] Handle errors

### 2.13 Implement Client Connection Methods

#### 2.13.1 Implement connect_with_timeout()
- [ ] Create socket if not exists
- [ ] Set non-blocking mode
- [ ] Call `connect(fd, addr, addrlen)`
- [ ] If returns -1 and errno == EINPROGRESS:
  - [ ] Use poll() or select() to wait with timeout
  - [ ] Check socket error with getsockopt(SO_ERROR)
  - [ ] If connected, return success
  - [ ] If timeout, throw NetworkException
- [ ] If returns 0, connection succeeded immediately
- [ ] Set `connected = true`
- [ ] Call setup()
- [ ] Determine MTU based on address family

### 2.14 Implement Reconnection Logic

#### 2.14.1 Implement reconnect()
- [ ] Only for client mode (server doesn't reconnect)
- [ ] Close existing socket: `close(fd)`
- [ ] Set `connected = false`
- [ ] Clear recv_buffer
- [ ] Log reconnection attempt if verbose
- [ ] Loop forever (infinite retries):
  - [ ] Create new socket
  - [ ] Try connect_with_timeout()
  - [ ] If success: break loop, set connected=true
  - [ ] If failure: wait RECONNECT_DELAY ms, retry
  - [ ] Log each attempt if verbose
- [ ] Log successful reconnection if verbose

### 2.15 Implement I/O Helper Methods

#### 2.15.1 Implement read_fully()
- [ ] Loop until exactly `len` bytes read:
  - [ ] Call `read(fd, buf + offset, len - offset)`
  - [ ] Handle return values:
    - [ ] n > 0: accumulate bytes, update offset
    - [ ] n == 0: connection closed, throw exception
    - [ ] n == -1:
      - [ ] EAGAIN/EWOULDBLOCK: use poll() to wait for data
      - [ ] ETIMEDOUT: throw timeout exception
      - [ ] ECONNRESET, EPIPE: connection lost, throw exception
      - [ ] Other errors: throw NetworkException
- [ ] Use poll() with tcp_timeout for blocking wait
- [ ] Return total bytes read

#### 2.15.2 Implement write_fully()
- [ ] Loop until exactly `len` bytes written:
  - [ ] Call `write(fd, buf + offset, len - offset)`
  - [ ] Handle return values:
    - [ ] n > 0: accumulate bytes, update offset
    - [ ] n == 0: shouldn't happen, throw exception
    - [ ] n == -1:
      - [ ] EAGAIN/EWOULDBLOCK: use poll() to wait for writability
      - [ ] ETIMEDOUT: throw timeout exception
      - [ ] EPIPE, ECONNRESET: connection lost, throw exception
      - [ ] Other errors: throw NetworkException
- [ ] Use poll() with tcp_timeout for blocking wait
- [ ] Return total bytes written

### 2.16 Implement Message Framing

#### 2.16.1 Implement send()
- [ ] Check if connected, if not try reconnect (client) or throw (server)
- [ ] Encrypt payload using session.encrypt()
- [ ] Get encrypted message string
- [ ] Calculate length: `uint32_t len = encrypted.size()`
- [ ] Convert to network byte order: `uint32_t net_len = htonl(len)`
- [ ] Try to send:
  - [ ] Write 4-byte length prefix using write_fully()
  - [ ] Write encrypted payload using write_fully()
  - [ ] If success: return
  - [ ] If failure (exception):
    - [ ] Store error in send_error
    - [ ] If client: attempt reconnect(), retry send
    - [ ] If server: throw exception
- [ ] Log send if very verbose (debugging)

#### 2.16.2 Implement recv_one()
- [ ] Check if connected, if not throw exception
- [ ] Loop until complete message:
  - [ ] If recv_buffer has < 4 bytes:
    - [ ] Read more data into recv_buffer
    - [ ] Use read() with non-blocking
    - [ ] Accumulate in buffer
  - [ ] If recv_buffer has >= 4 bytes:
    - [ ] Parse length: `uint32_t net_len = *(uint32_t*)recv_buffer.data()`
    - [ ] Convert to host order: `uint32_t len = ntohl(net_len)`
    - [ ] Validate length (< MAX_MESSAGE_SIZE)
    - [ ] If recv_buffer has >= 4 + len bytes:
      - [ ] Extract complete message
      - [ ] Remove from recv_buffer (including length prefix)
      - [ ] Decrypt message using session.decrypt()
      - [ ] Parse into Packet
      - [ ] Update RTT if needed
      - [ ] Update last_heard timestamp
      - [ ] Return decrypted payload
    - [ ] Else: read more data
- [ ] Handle partial reads gracefully
- [ ] Throw exception on connection errors

#### 2.16.3 Implement recv()
- [ ] If server and not connected:
  - [ ] Call accept_connection()
  - [ ] If timeout (no client yet), return empty string
- [ ] Try to recv_one():
  - [ ] If success: return message
  - [ ] If timeout: return empty string (expected, not an error)
  - [ ] If connection lost:
    - [ ] If client: attempt reconnect(), then return empty
    - [ ] If server: throw exception (can't reconnect)
- [ ] Update RTT tracking
- [ ] Log receive if very verbose

### 2.17 Implement Packet Creation

#### 2.17.1 Implement new_packet()
- [ ] Create Packet object with:
  - [ ] direction (TO_SERVER or TO_CLIENT)
  - [ ] current timestamp (Network::timestamp16())
  - [ ] saved_timestamp_reply (echo back peer's timestamp)
  - [ ] payload
- [ ] Return packet

### 2.18 Implement RTT Tracking

#### 2.18.1 Implement update_rtt()
- [ ] Extract timestamp_reply from received packet
- [ ] If timestamp_reply > 0:
  - [ ] Calculate round-trip time
  - [ ] If first RTT sample (!RTT_hit):
    - [ ] SRTT = R
    - [ ] RTTVAR = R / 2
    - [ ] RTT_hit = true
  - [ ] Else (update RTT per RFC 2988):
    - [ ] alpha = 1/8
    - [ ] beta = 1/4
    - [ ] RTTVAR = (1-beta)*RTTVAR + beta*|SRTT - R|
    - [ ] SRTT = (1-alpha)*SRTT + alpha*R
- [ ] Update saved_timestamp from received packet

### 2.19 Implement Interface Methods

#### 2.19.1 Implement fds()
- [ ] Create vector<int>
- [ ] Add fd to vector
- [ ] Return vector

#### 2.19.2 Implement timeout()
- [ ] Calculate RTO: `lrint(ceil(SRTT + 4 * RTTVAR))`
- [ ] Clamp between MIN_TCP_TIMEOUT and MAX_TCP_TIMEOUT
- [ ] Return RTO

#### 2.19.3 Implement get_MTU()
- [ ] Return MTU (DEFAULT_TCP_MTU)

#### 2.19.4 Implement port()
- [ ] If server: return bind_port
- [ ] If client: get local port from getsockname()
- [ ] Convert to string and return

#### 2.19.5 Implement get_key()
- [ ] Return key.printable_key()

#### 2.19.6 Implement get_has_remote_addr()
- [ ] Return has_remote_addr

#### 2.19.7 Implement get_SRTT()
- [ ] Return SRTT

#### 2.19.8 Implement set_last_roundtrip_success()
- [ ] Store value (used by Transport layer)

#### 2.19.9 Implement get_send_error()
- [ ] Return reference to send_error string

#### 2.19.10 Implement get_remote_addr()
- [ ] Return reference to remote_addr

#### 2.19.11 Implement get_remote_addr_len()
- [ ] Return remote_addr_len

### 2.20 Implement Utility Methods

#### 2.20.1 Implement set_timeout()
- [ ] Validate timeout value (MIN <= timeout <= MAX)
- [ ] Set tcp_timeout
- [ ] Update socket options if already connected

#### 2.20.2 Implement set_verbose()
- [ ] Set verbose member

#### 2.20.3 Implement close_connection()
- [ ] If fd >= 0:
  - [ ] Call shutdown(fd, SHUT_RDWR)
  - [ ] Call close(fd)
  - [ ] Set fd = -1
- [ ] Set connected = false

#### 2.20.4 Implement is_connected()
- [ ] Return connected flag
- [ ] Optionally: try to detect connection loss (poll with POLLHUP)

### 2.21 Error Handling

#### 2.21.1 Define TCPConnectionException
- [ ] Inherit from NetworkException
- [ ] Add connection state info
- [ ] Add errno info

#### 2.21.2 Add Try-Catch in Public Methods
- [ ] Wrap socket operations
- [ ] Convert system errors to exceptions
- [ ] Store error messages in send_error
- [ ] Log errors if verbose

### 2.22 Update Build System for TCP
- [ ] Add `tcpconnection.h` to Makefile.am
- [ ] Add `tcpconnection.cc` to Makefile.am
- [ ] Update dependencies

### 2.23 Phase 2 Testing

#### 2.23.1 Unit Tests
- [ ] Create `src/tests/test_tcpconnection.cc`
- [ ] Test: TCP socket creation
- [ ] Test: Server bind and listen
- [ ] Test: Client connect
- [ ] Test: Send and receive single message
- [ ] Test: Message framing (length prefix)
- [ ] Test: Partial message handling
- [ ] Test: Multiple messages in sequence
- [ ] Test: Timeout behavior
- [ ] Test: Connection failure detection
- [ ] Test: Error handling
- [ ] Test: MTU reporting
- [ ] Test: RTT calculation

#### 2.23.2 Build and Run Tests
- [ ] Compile with TCP support
- [ ] Fix compilation errors
- [ ] Run unit tests
- [ ] Fix failing tests
- [ ] Check for memory leaks (valgrind)

### 2.24 Phase 2 Cleanup
- [ ] Run clang-format
- [ ] Fix compiler warnings
- [ ] Add comments and documentation
- [ ] Review error handling paths
- [ ] Create git commit for Phase 2

---

## Phase 3: Connection Resilience & Reconnection

### 3.1 Enhance Reconnection Logic

#### 3.1.1 Add Connection State Machine
- [ ] Define enum `ConnectionState { DISCONNECTED, CONNECTING, CONNECTED, RECONNECTING }`
- [ ] Add `ConnectionState state` member
- [ ] Update state transitions
- [ ] Add state-specific behavior

#### 3.1.2 Implement Exponential Backoff
- [ ] Add `unsigned int reconnect_attempts` member
- [ ] Add `uint64_t reconnect_delay` member
- [ ] Implement backoff: `delay = min(RECONNECT_DELAY * 2^attempts, MAX_RECONNECT_DELAY)`
- [ ] Reset attempts on successful connection
- [ ] Add max delay constant: `MAX_RECONNECT_DELAY = 5000` ms

#### 3.1.3 Add Jitter to Reconnection
- [ ] Add random jitter to prevent thundering herd
- [ ] Use PRNG for jitter calculation
- [ ] Jitter range: ±25% of base delay

### 3.2 Improve Connection Monitoring

#### 3.2.1 Add Connection Health Checks
- [ ] Implement `bool check_connection_health()`
- [ ] Use poll() with POLLHUP/POLLERR to detect failures
- [ ] Call periodically in recv()
- [ ] Trigger reconnect if unhealthy

#### 3.2.2 Add Application-Level Keepalives
- [ ] Track time since last successful send/recv
- [ ] If idle > KEEPALIVE_TIMEOUT, send keepalive
- [ ] Reuse existing ACK mechanism (empty ack)
- [ ] Detect keepalive failure (timeout)

### 3.3 Improve Error Recovery

#### 3.3.1 Classify Errors
- [ ] Transient errors: EAGAIN, EINTR → retry
- [ ] Connection errors: ECONNRESET, EPIPE → reconnect
- [ ] Fatal errors: EINVAL, EBADF → throw exception
- [ ] Timeout errors: ETIMEDOUT → reconnect or retry

#### 3.3.2 Add Error Recovery Strategies
- [ ] For transient errors: immediate retry
- [ ] For connection errors: trigger reconnect
- [ ] For timeouts: count consecutive timeouts, reconnect after N
- [ ] For fatal errors: propagate to application

#### 3.3.3 Add Error Logging
- [ ] Log all connection errors if verbose
- [ ] Include errno and error message
- [ ] Track error counts
- [ ] Add diagnostic info (state, attempts, etc.)

### 3.4 Handle Partial I/O Robustly

#### 3.4.1 Improve read_fully()
- [ ] Add timeout tracking across partial reads
- [ ] Limit total time spent in function
- [ ] Handle signals (EINTR) gracefully
- [ ] Return partial data on connection loss?

#### 3.4.2 Improve write_fully()
- [ ] Add timeout tracking across partial writes
- [ ] Limit total time spent in function
- [ ] Handle signals (EINTR) gracefully
- [ ] Handle write buffer full (EAGAIN)

#### 3.4.3 Buffer Management
- [ ] Limit recv_buffer size (prevent memory exhaustion)
- [ ] Clear buffer on reconnection
- [ ] Handle very large messages (> MAX_MESSAGE_SIZE)

### 3.5 Add Connection Metrics

#### 3.5.1 Track Connection Statistics
- [ ] Add `uint64_t total_reconnects` counter
- [ ] Add `uint64_t total_bytes_sent` counter
- [ ] Add `uint64_t total_bytes_received` counter
- [ ] Add `uint64_t connection_established_time`
- [ ] Add `uint64_t last_successful_send`
- [ ] Add `uint64_t last_successful_recv`

#### 3.5.2 Add Diagnostic Methods
- [ ] `void print_stats()` - print connection statistics
- [ ] `uint64_t uptime()` - time since connection established
- [ ] `bool is_stale()` - connection idle for too long?

### 3.6 Add Graceful Shutdown

#### 3.6.1 Implement Graceful Close
- [ ] Send any pending data before close
- [ ] Call `shutdown(fd, SHUT_WR)` before close
- [ ] Wait briefly for peer acknowledgment
- [ ] Then call `close(fd)`

#### 3.6.2 Handle Shutdown Sequence
- [ ] Detect peer shutdown (recv returns 0)
- [ ] Send final ACK if needed
- [ ] Clean up resources
- [ ] Set state to DISCONNECTED

### 3.7 Phase 3 Testing

#### 3.7.1 Resilience Tests
- [ ] Test: Reconnect after server restart
- [ ] Test: Reconnect after network failure
- [ ] Test: Exponential backoff behavior
- [ ] Test: Jitter in reconnection timing
- [ ] Test: Connection health detection
- [ ] Test: Graceful shutdown

#### 3.7.2 Stress Tests
- [ ] Test: Many rapid reconnections
- [ ] Test: Send/recv during reconnection
- [ ] Test: Large message handling
- [ ] Test: Buffer overflow prevention
- [ ] Test: Memory leak during reconnects

#### 3.7.3 Error Injection Tests
- [ ] Test: Simulate ECONNRESET
- [ ] Test: Simulate ETIMEDOUT
- [ ] Test: Simulate EPIPE
- [ ] Test: Simulate partial reads/writes
- [ ] Test: Simulate slow peer

### 3.8 Phase 3 Cleanup
- [ ] Review all error paths
- [ ] Add comprehensive logging
- [ ] Update documentation
- [ ] Run clang-format
- [ ] Create git commit for Phase 3

---

## Phase 4: CLI Integration & Protocol Selection

### 4.1 Define Protocol Enum

#### 4.1.1 Create Protocol Type
- [ ] Create `src/network/protocol.h`
- [ ] Define `enum class TransportProtocol { UDP, TCP }`
- [ ] Add `const char* to_string(TransportProtocol)` function
- [ ] Add `TransportProtocol from_string(const char*)` function
- [ ] Add validation function

### 4.2 Update Transport Factory Methods

#### 4.2.1 Implement create_udp()
- [ ] Take all necessary parameters
- [ ] Allocate UDPConnection
- [ ] Create Transport with UDP connection
- [ ] Return Transport pointer

#### 4.2.2 Implement create_tcp()
- [ ] Take all necessary parameters
- [ ] Allocate TCPConnection
- [ ] Create Transport with TCP connection
- [ ] Return Transport pointer

#### 4.2.3 Add Generic Factory
- [ ] `static Transport* create(TransportProtocol, ...)`
- [ ] Switch on protocol type
- [ ] Call appropriate factory method

### 4.3 Update mosh-server

#### 4.3.1 Add Protocol Command-Line Option
- [ ] Open `src/frontend/mosh-server.cc`
- [ ] Add `--protocol` to usage message
- [ ] Add `--protocol=<tcp|udp>` to options parsing
- [ ] Store protocol choice
- [ ] Add validation

#### 4.3.2 Add TCP Timeout Option
- [ ] Add `--tcp-timeout` to usage message
- [ ] Add `--tcp-timeout=<ms>` to options parsing
- [ ] Validate range (100-1000)
- [ ] Store timeout value

#### 4.3.3 Create Transport with Selected Protocol
- [ ] Based on protocol flag, call appropriate factory
- [ ] Pass tcp_timeout if TCP selected
- [ ] Handle creation errors

#### 4.3.4 Update MOSH CONNECT Output
- [ ] Change format to: `MOSH CONNECT <protocol> <port> <key>`
- [ ] Example: `MOSH CONNECT tcp 60001 abcd...`
- [ ] Maintain backward compatibility (default UDP)

#### 4.3.5 Environment Variable Support
- [ ] Check `MOSH_PROTOCOL` environment variable
- [ ] Override with command-line flag if provided
- [ ] Check `MOSH_TCP_TIMEOUT` for timeout
- [ ] Validate all environment values

### 4.4 Update mosh-client

#### 4.4.1 Add Protocol Parsing
- [ ] Open `src/frontend/mosh-client.cc`
- [ ] Read `MOSH_PROTOCOL` from environment
- [ ] Default to UDP if not set
- [ ] Validate protocol string

#### 4.4.2 Add TCP Timeout Parsing
- [ ] Read `MOSH_TCP_TIMEOUT` from environment
- [ ] Parse to integer
- [ ] Validate range
- [ ] Default to 500ms

#### 4.4.3 Create Transport with Selected Protocol
- [ ] Based on MOSH_PROTOCOL, call appropriate factory
- [ ] Pass tcp_timeout if TCP
- [ ] Handle creation errors
- [ ] Log protocol choice if verbose

#### 4.4.4 Update Error Messages
- [ ] Include protocol in connection error messages
- [ ] Suggest trying different protocol on failure
- [ ] Add troubleshooting hints

### 4.5 Update mosh Wrapper Script

#### 4.5.1 Add Protocol Option to mosh.pl
- [ ] Open `scripts/mosh.pl`
- [ ] Add `--protocol=<tcp|udp>` option
- [ ] Add to GetOptions()
- [ ] Add to usage/help message
- [ ] Default to 'udp'

#### 4.5.2 Add TCP Timeout Option
- [ ] Add `--tcp-timeout=<ms>` option
- [ ] Validate range
- [ ] Store value

#### 4.5.3 Pass Protocol to Server
- [ ] Build mosh-server command
- [ ] Add `--protocol=$protocol` to command
- [ ] Add `--tcp-timeout=$tcp_timeout` if TCP

#### 4.5.4 Parse Extended MOSH CONNECT Output
- [ ] Update regex to match: `MOSH CONNECT <proto> <port> <key>`
- [ ] Extract protocol from output
- [ ] Fall back to 2-field format for compatibility
- [ ] Validate protocol matches request

#### 4.5.5 Set Environment for Client
- [ ] Set `MOSH_PROTOCOL=$protocol`
- [ ] Set `MOSH_TCP_TIMEOUT=$timeout` if TCP
- [ ] Keep existing MOSH_KEY setting

#### 4.5.6 Handle Protocol Mismatch
- [ ] If server returns different protocol, warn user
- [ ] If server doesn't support protocol, show error
- [ ] Suggest fallback options

### 4.6 Update STMClient

#### 4.6.1 Add Protocol Support to STMClient
- [ ] Open `src/frontend/stmclient.h`
- [ ] Add protocol member if needed
- [ ] Update constructor to accept protocol
- [ ] Open `src/frontend/stmclient.cc`
- [ ] Update main_init() to create appropriate transport

### 4.7 Add Configuration Validation

#### 4.7.1 Validate Protocol Selection
- [ ] Check protocol is "tcp" or "udp"
- [ ] Reject invalid values
- [ ] Provide clear error messages

#### 4.7.2 Validate TCP Timeout
- [ ] Check range 100-1000ms
- [ ] Warn if outside recommended range
- [ ] Apply bounds

#### 4.7.3 Validate Compatibility
- [ ] Check server version supports protocol
- [ ] Warn about old clients/servers
- [ ] Provide upgrade guidance

### 4.8 Phase 4 Testing

#### 4.8.1 Command-Line Tests
- [ ] Test: `mosh --protocol=udp user@host`
- [ ] Test: `mosh --protocol=tcp user@host`
- [ ] Test: `mosh --tcp-timeout=300 user@host`
- [ ] Test: Invalid protocol value
- [ ] Test: Invalid timeout value
- [ ] Test: Default protocol (udp)

#### 4.8.2 Environment Variable Tests
- [ ] Test: `MOSH_PROTOCOL=tcp mosh user@host`
- [ ] Test: `MOSH_TCP_TIMEOUT=800 mosh user@host`
- [ ] Test: Command-line overrides environment
- [ ] Test: Invalid environment values

#### 4.8.3 Integration Tests
- [ ] Test: Full TCP session client-to-server
- [ ] Test: Full UDP session (ensure unchanged)
- [ ] Test: Protocol negotiation via SSH
- [ ] Test: MOSH CONNECT parsing
- [ ] Test: Client receives correct protocol

#### 4.8.4 Compatibility Tests
- [ ] Test: New client, old server (UDP only)
- [ ] Test: Old client, new server
- [ ] Test: Both old (UDP only)
- [ ] Test: Both new with UDP
- [ ] Test: Both new with TCP

### 4.9 Phase 4 Cleanup
- [ ] Update all help messages
- [ ] Update usage strings
- [ ] Add protocol to verbose output
- [ ] Run clang-format
- [ ] Create git commit for Phase 4

---

## Phase 5: Testing & Validation

### 5.1 Unit Tests

#### 5.1.1 TCP Connection Tests
- [ ] Test socket creation
- [ ] Test server bind
- [ ] Test client connect
- [ ] Test send/recv single message
- [ ] Test send/recv multiple messages
- [ ] Test message framing
- [ ] Test partial messages
- [ ] Test timeout behavior
- [ ] Test MTU reporting

#### 5.1.2 Reconnection Tests
- [ ] Test reconnect after disconnect
- [ ] Test exponential backoff
- [ ] Test reconnect jitter
- [ ] Test max reconnect attempts (if any)
- [ ] Test reconnect success
- [ ] Test reconnect failure handling

#### 5.1.3 Error Handling Tests
- [ ] Test ECONNRESET handling
- [ ] Test EPIPE handling
- [ ] Test ETIMEDOUT handling
- [ ] Test ECONNREFUSED handling
- [ ] Test invalid data handling

#### 5.1.4 RTT Tests
- [ ] Test RTT calculation
- [ ] Test SRTT update
- [ ] Test RTTVAR update
- [ ] Test timeout calculation from RTT

### 5.2 Integration Tests

#### 5.2.1 End-to-End TCP Tests
- [ ] Test complete client-server session over TCP
- [ ] Test terminal I/O over TCP
- [ ] Test interactive session
- [ ] Test non-interactive commands
- [ ] Test large output
- [ ] Test rapid input

#### 5.2.2 UDP Regression Tests
- [ ] Test complete client-server session over UDP
- [ ] Verify UDP behavior unchanged
- [ ] Test all UDP-specific features (port hopping)
- [ ] Compare UDP performance before/after changes

#### 5.2.3 Protocol Selection Tests
- [ ] Test explicit UDP selection
- [ ] Test explicit TCP selection
- [ ] Test default protocol (UDP)
- [ ] Test protocol via environment
- [ ] Test protocol via command line

#### 5.2.4 Network Condition Tests
- [ ] Test on localhost (low latency)
- [ ] Test over WAN (high latency)
- [ ] Test through VPN
- [ ] Test with packet loss (using tc/netem)
- [ ] Test with latency variation
- [ ] Test with bandwidth limits

### 5.3 Performance Tests

#### 5.3.1 Latency Tests
- [ ] Measure TCP latency (localhost)
- [ ] Measure UDP latency (localhost)
- [ ] Compare TCP vs UDP latency
- [ ] Measure latency over WAN
- [ ] Measure latency through VPN
- [ ] Test latency under load

#### 5.3.2 Throughput Tests
- [ ] Measure TCP throughput
- [ ] Measure UDP throughput
- [ ] Compare throughput
- [ ] Test with large file transfers
- [ ] Test with rapid terminal updates

#### 5.3.3 Resource Usage Tests
- [ ] Measure CPU usage (TCP vs UDP)
- [ ] Measure memory usage
- [ ] Test memory leaks (valgrind)
- [ ] Test file descriptor leaks
- [ ] Test under sustained load

#### 5.3.4 Scalability Tests
- [ ] Test multiple concurrent connections
- [ ] Test server under load
- [ ] Test reconnection storms
- [ ] Test resource limits

### 5.4 Reliability Tests

#### 5.4.1 Connection Failure Tests
- [ ] Kill server during session
- [ ] Kill client during session
- [ ] Restart server
- [ ] Restart client
- [ ] Network cable unplug/replug
- [ ] Suspend/resume laptop

#### 5.4.2 Long-Running Tests
- [ ] 24-hour stability test (TCP)
- [ ] 24-hour stability test (UDP)
- [ ] Test connection persistence
- [ ] Test keepalive effectiveness
- [ ] Monitor for leaks or degradation

#### 5.4.3 Stress Tests
- [ ] Rapid connect/disconnect cycles
- [ ] Send very large messages
- [ ] Send at maximum rate
- [ ] Test buffer overflow protection
- [ ] Test with corrupted packets

### 5.5 Security Tests

#### 5.5.1 Encryption Tests
- [ ] Verify all TCP traffic encrypted
- [ ] Verify key exchange secure
- [ ] Test with invalid keys
- [ ] Test with key mismatch
- [ ] Verify no plaintext leakage

#### 5.5.2 Attack Resistance Tests
- [ ] Test against SYN flood (TCP)
- [ ] Test against connection exhaustion
- [ ] Test against message injection
- [ ] Test against replay attacks
- [ ] Test against malformed messages

### 5.6 Compatibility Tests

#### 5.6.1 Version Compatibility
- [ ] Test new client with old server
- [ ] Test old client with new server
- [ ] Test protocol fallback
- [ ] Test version negotiation

#### 5.6.2 Platform Tests
- [ ] Test on Linux
- [ ] Test on macOS
- [ ] Test on FreeBSD
- [ ] Test on other Unix variants
- [ ] Test on different architectures (x86, ARM)

#### 5.6.3 Network Environment Tests
- [ ] Test through corporate firewall
- [ ] Test through home NAT
- [ ] Test through VPN (various types)
- [ ] Test through proxy
- [ ] Test in IPv4-only network
- [ ] Test in IPv6-only network
- [ ] Test in dual-stack network

### 5.7 User Experience Tests

#### 5.7.1 Usability Tests
- [ ] Test default behavior (should work without flags)
- [ ] Test error messages clarity
- [ ] Test verbose output usefulness
- [ ] Test troubleshooting workflow

#### 5.7.2 Documentation Tests
- [ ] Verify examples in docs work
- [ ] Test all documented flags
- [ ] Verify man pages accurate
- [ ] Check README instructions

### 5.8 Automated Test Suite

#### 5.8.1 Create Test Framework
- [ ] Set up test harness
- [ ] Add test runner script
- [ ] Integrate with existing tests
- [ ] Add CI configuration

#### 5.8.2 Add Test Cases
- [ ] Convert manual tests to automated
- [ ] Add regression tests
- [ ] Add smoke tests
- [ ] Add nightly tests

### 5.9 Phase 5 Deliverables
- [ ] All tests passing
- [ ] Test coverage report
- [ ] Performance benchmark report
- [ ] Compatibility matrix
- [ ] Known issues document
- [ ] Create git commit for Phase 5

---

## Phase 6: Documentation

### 6.1 Man Pages

#### 6.1.1 Update mosh Man Page
- [ ] Open `man/mosh.1`
- [ ] Add `--protocol` option to OPTIONS section
- [ ] Add `--tcp-timeout` option
- [ ] Add EXAMPLES section with TCP
- [ ] Add notes about when to use TCP vs UDP
- [ ] Document environment variables
- [ ] Update SEE ALSO section

#### 6.1.2 Update mosh-server Man Page
- [ ] Open `man/mosh-server.1`
- [ ] Add `--protocol` option to OPTIONS
- [ ] Add `--tcp-timeout` option
- [ ] Add EXAMPLES with TCP
- [ ] Document MOSH CONNECT format changes

#### 6.1.3 Update mosh-client Man Page
- [ ] Open `man/mosh-client.1`
- [ ] Document MOSH_PROTOCOL environment variable
- [ ] Document MOSH_TCP_TIMEOUT environment variable
- [ ] Add examples

### 6.2 README Updates

#### 6.2.1 Update Main README
- [ ] Open `README.md`
- [ ] Add section on TCP support
- [ ] Add "Why TCP?" explanation
- [ ] Add usage examples
- [ ] Update feature list
- [ ] Add performance notes
- [ ] Add compatibility notes

#### 6.2.2 Add TCP Usage Examples
- [ ] Basic TCP usage
- [ ] TCP through VPN
- [ ] TCP with custom timeout
- [ ] When to use TCP vs UDP
- [ ] Troubleshooting TCP connections

### 6.3 Technical Documentation

#### 6.3.1 Update PROTOCOL Documentation
- [ ] Document TCP transport layer
- [ ] Document message framing format
- [ ] Document connection lifecycle
- [ ] Document reconnection behavior
- [ ] Add TCP-specific diagrams

#### 6.3.2 Create TCP Design Document
- [ ] Architecture overview
- [ ] Design decisions
- [ ] Tradeoffs (TCP vs UDP)
- [ ] Future enhancements
- [ ] References

#### 6.3.3 Create Troubleshooting Guide
- [ ] Common TCP issues
- [ ] Debugging connection problems
- [ ] Performance tuning
- [ ] Firewall configuration
- [ ] VPN compatibility

### 6.4 Code Documentation

#### 6.4.1 Add Header Comments
- [ ] Document ConnectionInterface
- [ ] Document TCPConnection class
- [ ] Document all public methods
- [ ] Add usage examples in comments

#### 6.4.2 Add Implementation Comments
- [ ] Comment complex algorithms
- [ ] Explain design choices
- [ ] Document assumptions
- [ ] Add TODO notes for future work

### 6.5 User-Facing Documentation

#### 6.5.1 Create FAQ
- [ ] What is TCP transport?
- [ ] When should I use TCP?
- [ ] How do I enable TCP?
- [ ] Is TCP slower than UDP?
- [ ] Does TCP support roaming?
- [ ] How do I troubleshoot TCP issues?

#### 6.5.2 Create Migration Guide
- [ ] For existing users
- [ ] For system administrators
- [ ] Deployment considerations
- [ ] Rollback procedures

#### 6.5.3 Update Website/Wiki
- [ ] Update feature page
- [ ] Add TCP tutorial
- [ ] Add performance comparison
- [ ] Update FAQ

### 6.6 Release Notes

#### 6.6.1 Write CHANGELOG Entry
- [ ] New feature: TCP transport support
- [ ] New flags: --protocol, --tcp-timeout
- [ ] New environment variables
- [ ] Backward compatibility notes
- [ ] Breaking changes (if any)
- [ ] Upgrade instructions

#### 6.6.2 Write Release Announcement
- [ ] Highlight TCP support
- [ ] Explain benefits
- [ ] Provide examples
- [ ] Link to documentation
- [ ] Acknowledge contributors

### 6.7 Developer Documentation

#### 6.7.1 Update Contributing Guide
- [ ] How to add new transport protocols
- [ ] Testing requirements
- [ ] Code style for network code

#### 6.7.2 Update Build Instructions
- [ ] Any new dependencies
- [ ] New build options
- [ ] Testing procedures

### 6.8 Phase 6 Deliverables
- [ ] All documentation updated
- [ ] Man pages complete
- [ ] README updated
- [ ] Examples verified
- [ ] Create git commit for Phase 6

---

## Phase 7: Final Integration & Release Preparation

### 7.1 Code Review

#### 7.1.1 Self-Review
- [ ] Review all changed files
- [ ] Check for code duplication
- [ ] Verify error handling complete
- [ ] Check for memory leaks
- [ ] Verify thread safety (if applicable)
- [ ] Check for race conditions

#### 7.1.2 Security Review
- [ ] Review encryption implementation
- [ ] Check for buffer overflows
- [ ] Verify input validation
- [ ] Check for timing attacks
- [ ] Review authentication flow

#### 7.1.3 Performance Review
- [ ] Profile TCP code
- [ ] Identify bottlenecks
- [ ] Optimize hot paths
- [ ] Minimize allocations
- [ ] Reduce copies

### 7.2 Build System Integration

#### 7.2.1 Update Autotools
- [ ] Update `configure.ac`
- [ ] Update `Makefile.am` files
- [ ] Add feature detection
- [ ] Add build options
- [ ] Test on clean system

#### 7.2.2 Update CMake (if used)
- [ ] Update `CMakeLists.txt`
- [ ] Add TCP source files
- [ ] Update install targets

#### 7.2.3 Update Package Manifests
- [ ] Update Debian packaging
- [ ] Update RPM spec file
- [ ] Update Homebrew formula
- [ ] Update other package formats

### 7.3 Continuous Integration

#### 7.3.1 Update CI Configuration
- [ ] Add TCP tests to CI
- [ ] Test on multiple platforms
- [ ] Add performance benchmarks
- [ ] Add coverage reporting

#### 7.3.2 Add Pre-Commit Hooks
- [ ] Run tests before commit
- [ ] Check code formatting
- [ ] Run static analysis

### 7.4 Final Testing

#### 7.4.1 Full Test Suite
- [ ] Run all unit tests
- [ ] Run all integration tests
- [ ] Run performance tests
- [ ] Run compatibility tests
- [ ] Run stress tests

#### 7.4.2 Platform Testing
- [ ] Test on Linux (multiple distros)
- [ ] Test on macOS
- [ ] Test on BSD variants
- [ ] Test on different architectures

#### 7.4.3 Regression Testing
- [ ] Verify UDP still works
- [ ] Test all existing features
- [ ] Check for performance regressions

### 7.5 Beta Testing

#### 7.5.1 Internal Beta
- [ ] Deploy to test environment
- [ ] Dogfood with team
- [ ] Collect feedback
- [ ] Fix issues

#### 7.5.2 External Beta
- [ ] Release beta version
- [ ] Announce to community
- [ ] Collect bug reports
- [ ] Iterate based on feedback

### 7.6 Release Preparation

#### 7.6.1 Version Bump
- [ ] Update version number
- [ ] Update protocol version if needed
- [ ] Update copyright years

#### 7.6.2 Final Documentation
- [ ] Proofread all docs
- [ ] Update screenshots
- [ ] Verify all links
- [ ] Update translations (if any)

#### 7.6.3 Release Artifacts
- [ ] Create source tarball
- [ ] Create binary packages
- [ ] Sign releases
- [ ] Upload to distribution sites

#### 7.6.4 Release Notes
- [ ] Write comprehensive release notes
- [ ] List all changes
- [ ] Credit contributors
- [ ] Document known issues

### 7.7 Communication

#### 7.7.1 Announce Release
- [ ] Post to mailing list
- [ ] Post to website
- [ ] Social media announcement
- [ ] Update relevant forums

#### 7.7.2 Update Documentation Sites
- [ ] Update official docs
- [ ] Update wiki
- [ ] Update third-party references

### 7.8 Post-Release

#### 7.8.1 Monitor Issues
- [ ] Watch bug tracker
- [ ] Monitor mailing list
- [ ] Check social media
- [ ] Respond to questions

#### 7.8.2 Plan Next Steps
- [ ] Review feedback
- [ ] Identify improvements
- [ ] Plan future enhancements
- [ ] Update roadmap

### 7.9 Phase 7 Deliverables
- [ ] Clean, reviewed code
- [ ] All tests passing
- [ ] Complete documentation
- [ ] Release artifacts
- [ ] Release announcement
- [ ] Create final git commit
- [ ] Create git tag for release

---

## Appendix: Reference Tasks

### A.1 Code Quality Checklist
- [ ] All files have copyright headers
- [ ] Code follows style guide (clang-format)
- [ ] No compiler warnings
- [ ] No static analyzer warnings
- [ ] Pass valgrind (no leaks)
- [ ] Pass thread sanitizer (if threaded)
- [ ] Pass address sanitizer
- [ ] Pass undefined behavior sanitizer

### A.2 Testing Checklist
- [ ] Unit test coverage > 80%
- [ ] All edge cases tested
- [ ] Error paths tested
- [ ] Performance benchmarks run
- [ ] No test failures
- [ ] No flaky tests

### A.3 Documentation Checklist
- [ ] All public APIs documented
- [ ] Man pages complete
- [ ] README up to date
- [ ] Examples working
- [ ] No broken links
- [ ] Changelog updated

### A.4 Security Checklist
- [ ] All inputs validated
- [ ] No buffer overflows
- [ ] Proper error handling
- [ ] No information leaks
- [ ] Secure defaults
- [ ] Encryption verified

### A.5 Compatibility Checklist
- [ ] Backward compatible (or documented)
- [ ] Works on all target platforms
- [ ] Works with old clients/servers
- [ ] Migration path clear

---

## Notes

### Task Estimation
- Phase 1: ~1 week (refactoring, no new functionality)
- Phase 2: ~1 week (TCP core implementation)
- Phase 3: ~3-4 days (resilience features)
- Phase 4: ~3-4 days (CLI integration)
- Phase 5: ~1 week (comprehensive testing)
- Phase 6: ~2-3 days (documentation)
- Phase 7: ~3-4 days (release prep)
- **Total: ~4-5 weeks**

### Dependencies
- Phase 2 depends on Phase 1
- Phase 3 depends on Phase 2
- Phase 4 depends on Phases 2-3
- Phase 5 can start after Phase 4
- Phase 6 can start after Phase 4
- Phase 7 depends on Phases 5-6

### Parallel Work
- Documentation (Phase 6) can be done in parallel with testing (Phase 5)
- Some Phase 3 tasks can overlap with Phase 2
- Testing should be continuous throughout

### Risk Areas
- ⚠️ Message framing correctness (partial reads)
- ⚠️ Reconnection logic complexity
- ⚠️ Backward compatibility
- ⚠️ Performance regressions
- ⚠️ Platform-specific socket behavior

### Success Criteria
- ✅ TCP works through UDP-blocking VPNs
- ✅ UDP behavior unchanged
- ✅ Tests pass on all platforms
- ✅ Performance acceptable (within 10% of UDP)
- ✅ Documentation complete
- ✅ No security regressions
