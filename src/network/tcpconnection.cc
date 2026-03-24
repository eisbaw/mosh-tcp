/*
    Mosh: the mobile shell
    Copyright 2012 Keith Winstein

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    In addition, as a special exception, the copyright holders give
    permission to link the code of portions of this program with the
    OpenSSL library under certain conditions as described in each
    individual source file, and distribute linked combinations including
    the two.

    You must obey the GNU General Public License in all respects for all
    of the code used other than OpenSSL. If you modify file(s) with this
    exception, you may extend this exception to your version of the
    file(s), but you are not obligated to do so. If you do not wish to do
    so, delete this exception statement from your version. If you delete
    this exception statement from all source files in the program, then
    also delete it here.
*/

#include "tcpconnection.h"
#include "network.h"
#include "src/util/timestamp.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

using namespace Network;
using namespace Crypto;

/* Server constructor */
TCPConnection::TCPConnection( const char* desired_ip, const char* desired_port )
  : fd( -1 ),
    listen_fd( -1 ),
    server( true ),
    connected( false ),
    reconnecting( false ),
    reconnect_attempt( 0 ),
    next_reconnect_time( 0 ),
    remote_addr(),
    remote_addr_len( 0 ),
    has_remote_addr( false ),
    bind_ip( desired_ip ? desired_ip : "" ),
    bind_port( desired_port ? desired_port : "" ),
    MTU( DEFAULT_TCP_MTU ),
    tcp_timeout( DEFAULT_TCP_TIMEOUT ),
    key(),
    session( key ),
    direction( TO_CLIENT ),
    saved_timestamp( 0 ),
    saved_timestamp_received_at( 0 ),
    expected_receiver_seq( 0 ),
    RTT_hit( false ),
    SRTT( 1000 ),
    RTTVAR( 500 ),
    last_heard( 0 ),
    last_roundtrip_success( 0 ),
    send_error(),
    recv_buffer(),
    send_buffer(),
    verbose( 0 )
{
  /* Bind and listen for client connections */
  bind_and_listen( desired_ip, desired_port );

  /* Initialize timestamps */
  last_heard = timestamp();
}

/* Client constructor */
TCPConnection::TCPConnection( const char* key_str, const char* ip, const char* port )
  : fd( -1 ),
    listen_fd( -1 ),
    server( false ),
    connected( false ),
    reconnecting( false ),
    reconnect_attempt( 0 ),
    next_reconnect_time( 0 ),
    remote_addr(),
    remote_addr_len( 0 ),
    has_remote_addr( false ),
    bind_ip(),
    bind_port(),
    MTU( DEFAULT_TCP_MTU ),
    tcp_timeout( DEFAULT_TCP_TIMEOUT ),
    key( key_str ),
    session( key ),
    direction( TO_SERVER ),
    saved_timestamp( 0 ),
    saved_timestamp_received_at( 0 ),
    expected_receiver_seq( 0 ),
    RTT_hit( false ),
    SRTT( 1000 ),
    RTTVAR( 500 ),
    last_heard( 0 ),
    last_roundtrip_success( 0 ),
    send_error(),
    recv_buffer(),
    send_buffer(),
    verbose( 0 )
{
  /* Resolve server address */
  struct addrinfo hints, *res;
  memset( &hints, 0, sizeof( hints ) );
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  int err = getaddrinfo( ip, port, &hints, &res );
  if ( err != 0 ) {
    throw NetworkException( std::string( "getaddrinfo: " ) + gai_strerror( err ), 0 );
  }

  /* Store remote address */
  memcpy( &remote_addr, res->ai_addr, res->ai_addrlen );
  remote_addr_len = res->ai_addrlen;
  has_remote_addr = true;

  freeaddrinfo( res );

  /* Connect to server */
  connect_with_timeout( remote_addr, CONNECT_TIMEOUT );

  /* Initialize timestamps */
  last_heard = timestamp();
}

/* Destructor */
TCPConnection::~TCPConnection()
{
  close_connection();
  if ( listen_fd >= 0 ) {
    close( listen_fd );
  }
}

/* Bind and listen for connections (server mode) */
void TCPConnection::bind_and_listen( const char* ip, const char* port )
{
  struct addrinfo hints, *res;
  memset( &hints, 0, sizeof( hints ) );
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int err = getaddrinfo( ip, port, &hints, &res );
  if ( err != 0 ) {
    throw NetworkException( std::string( "getaddrinfo: " ) + gai_strerror( err ), 0 );
  }

  /* Create socket */
  listen_fd = socket( res->ai_family, res->ai_socktype, res->ai_protocol );
  if ( listen_fd < 0 ) {
    int saved_errno = errno;
    freeaddrinfo( res );
    throw NetworkException( "socket", saved_errno );
  }

  /* Set SO_REUSEADDR */
  int optval = 1;
  if ( setsockopt( listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof( optval ) ) < 0 ) {
    int saved_errno = errno;
    close( listen_fd );
    freeaddrinfo( res );
    throw NetworkException( "setsockopt(SO_REUSEADDR)", saved_errno );
  }

  /* Bind */
  if ( bind( listen_fd, res->ai_addr, res->ai_addrlen ) < 0 ) {
    int saved_errno = errno;
    close( listen_fd );
    freeaddrinfo( res );
    throw NetworkException( "bind", saved_errno );
  }

  /* Get the actual port if port was "0" */
  Addr local_addr;
  socklen_t local_addr_len = sizeof( local_addr );
  if ( getsockname( listen_fd, &local_addr.sa, &local_addr_len ) < 0 ) {
    int saved_errno = errno;
    close( listen_fd );
    freeaddrinfo( res );
    throw NetworkException( "getsockname", saved_errno );
  }

  if ( local_addr.sa.sa_family == AF_INET ) {
    char port_str[16];
    snprintf( port_str, sizeof( port_str ), "%d", ntohs( local_addr.sin.sin_port ) );
    bind_port = port_str;
  } else if ( local_addr.sa.sa_family == AF_INET6 ) {
    char port_str[16];
    snprintf( port_str, sizeof( port_str ), "%d", ntohs( local_addr.sin6.sin6_port ) );
    bind_port = port_str;
  }

  freeaddrinfo( res );

  /* Listen */
  if ( listen( listen_fd, 1 ) < 0 ) {
    int saved_errno = errno;
    close( listen_fd );
    throw NetworkException( "listen", saved_errno );
  }

  if ( verbose > 0 ) {
    fprintf( stderr, "[TCP] Server listening on port %s\n", bind_port.c_str() );
  }
}

/* Accept client connection (server mode) */
void TCPConnection::accept_connection( void )
{
  /* Use poll with short timeout to avoid blocking forever */
  struct pollfd pfd;
  pfd.fd = listen_fd;
  pfd.events = POLLIN;

  int ret = poll( &pfd, 1, tcp_timeout );
  if ( ret < 0 ) {
    if ( errno == EINTR ) {
      return; /* Try again */
    }
    throw NetworkException( "poll", errno );
  }

  if ( ret == 0 ) {
    /* Timeout - no client yet */
    return;
  }

  /* Accept connection */
  remote_addr_len = sizeof( remote_addr ); /* Initialize before accept() - required by accept(2) */
  fd = accept( listen_fd, &remote_addr.sa, &remote_addr_len );
  if ( fd < 0 ) {
    if ( errno == EAGAIN || errno == EWOULDBLOCK ) {
      return; /* Try again */
    }
    throw NetworkException( "accept", errno );
  }

  connected = true;
  has_remote_addr = true;

  /* Keep listen_fd open so we can accept reconnections later */

  /* Clear buffers to discard partial framing data from the old TCP stream.
   * The Crypto::Session is intentionally preserved -- see reconnect() comment
   * for the safety analysis.  The process-global nonce counter (Crypto::unique())
   * ensures post-reconnect messages use fresh, unique nonces. */
  recv_buffer.clear();
  send_buffer.clear();

  /* Setup socket options */
  try {
    setup();
  } catch ( ... ) {
    /* Cleanup on setup failure */
    close( fd );
    fd = -1;
    connected = false;
    has_remote_addr = false;
    throw;
  }

  if ( verbose > 0 ) {
    char addr_str[INET6_ADDRSTRLEN];
    const char* result = NULL;
    if ( remote_addr.sa.sa_family == AF_INET ) {
      result = inet_ntop( AF_INET, &remote_addr.sin.sin_addr, addr_str, sizeof( addr_str ) );
    } else if ( remote_addr.sa.sa_family == AF_INET6 ) {
      result = inet_ntop( AF_INET6, &remote_addr.sin6.sin6_addr, addr_str, sizeof( addr_str ) );
    }
    fprintf( stderr, "[TCP] Client connected from %s\n", result ? addr_str : "unknown" );
  }
}

/* Connect to server with timeout (client mode) */
void TCPConnection::connect_with_timeout( const Addr& addr, uint64_t timeout_ms )
{
  /* Create socket */
  fd = socket( addr.sa.sa_family, SOCK_STREAM, 0 );
  if ( fd < 0 ) {
    throw NetworkException( "socket", errno );
  }

  /* Set non-blocking */
  int flags = fcntl( fd, F_GETFL, 0 );
  if ( flags < 0 || fcntl( fd, F_SETFL, flags | O_NONBLOCK ) < 0 ) {
    int saved_errno = errno;
    close( fd );
    fd = -1;
    throw NetworkException( "fcntl(O_NONBLOCK)", saved_errno );
  }

  /* Attempt connection */
  int ret = connect( fd, &addr.sa, remote_addr_len );
  if ( ret < 0 && errno != EINPROGRESS ) {
    int saved_errno = errno;
    close( fd );
    fd = -1;
    throw NetworkException( "connect", saved_errno );
  }

  /* Wait for connection with timeout */
  if ( ret < 0 ) {
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLOUT;

    ret = poll( &pfd, 1, timeout_ms );
    if ( ret < 0 ) {
      int saved_errno = errno;
      close( fd );
      fd = -1;
      throw NetworkException( "poll", saved_errno );
    }

    if ( ret == 0 ) {
      /* Timeout */
      close( fd );
      fd = -1;
      throw NetworkException( "connect timeout", ETIMEDOUT );
    }

    /* Check if connection succeeded */
    int error;
    socklen_t error_len = sizeof( error );
    if ( getsockopt( fd, SOL_SOCKET, SO_ERROR, &error, &error_len ) < 0 ) {
      int saved_errno = errno;
      close( fd );
      fd = -1;
      throw NetworkException( "getsockopt(SO_ERROR)", saved_errno );
    }

    if ( error != 0 ) {
      close( fd );
      fd = -1;
      throw NetworkException( "connect", error );
    }
  }

  connected = true;

  /* Setup socket options */
  setup();

  if ( verbose > 0 ) {
    fprintf( stderr, "[TCP] Connected to server\n" );
  }
}

/* Setup socket options */
void TCPConnection::setup( void )
{
  /* Ensure socket is in non-blocking mode.
     Client path (connect_with_timeout) already sets this, but the server's
     accepted fd does not inherit non-blocking from the listen socket. */
  int flags = fcntl( fd, F_GETFL, 0 );
  if ( flags < 0 || fcntl( fd, F_SETFL, flags | O_NONBLOCK ) < 0 ) {
    throw NetworkException( "fcntl(O_NONBLOCK)", errno );
  }

  setup_socket_options();
  set_socket_timeout( tcp_timeout );
}

void TCPConnection::setup_socket_options( void )
{
  int flag = 1;

  /* Prevent SIGPIPE on macOS/BSD when writing to closed socket */
#ifdef SO_NOSIGPIPE
  if ( setsockopt( fd, SOL_SOCKET, SO_NOSIGPIPE, &flag, sizeof( flag ) ) < 0 ) {
    if ( verbose > 0 ) {
      fprintf( stderr, "[TCP] Warning: could not set SO_NOSIGPIPE: %s\n", strerror( errno ) );
    }
  }
#endif

  /* Enable TCP_NODELAY (disable Nagle's algorithm) */
  if ( setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof( flag ) ) < 0 ) {
    if ( verbose > 0 ) {
      fprintf( stderr, "[TCP] Warning: could not set TCP_NODELAY: %s\n", strerror( errno ) );
    }
  }

  /* Enable SO_KEEPALIVE */
  if ( setsockopt( fd, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof( flag ) ) < 0 ) {
    if ( verbose > 0 ) {
      fprintf( stderr, "[TCP] Warning: could not set SO_KEEPALIVE: %s\n", strerror( errno ) );
    }
  }

#ifdef TCP_KEEPIDLE
  /* Set keepalive parameters (Linux-specific) */
  int keepidle = 10; /* Start probing after 10s idle */
  if ( setsockopt( fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof( keepidle ) ) < 0 ) {
    if ( verbose > 1 ) {
      fprintf( stderr, "[TCP] Warning: could not set TCP_KEEPIDLE: %s\n", strerror( errno ) );
    }
  }
#endif

#ifdef TCP_KEEPINTVL
  int keepintvl = 3; /* Probe every 3s */
  if ( setsockopt( fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof( keepintvl ) ) < 0 ) {
    if ( verbose > 1 ) {
      fprintf( stderr, "[TCP] Warning: could not set TCP_KEEPINTVL: %s\n", strerror( errno ) );
    }
  }
#endif

#ifdef TCP_KEEPCNT
  int keepcnt = 3; /* 3 failed probes = connection dead */
  if ( setsockopt( fd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof( keepcnt ) ) < 0 ) {
    if ( verbose > 1 ) {
      fprintf( stderr, "[TCP] Warning: could not set TCP_KEEPCNT: %s\n", strerror( errno ) );
    }
  }
#endif
}

void TCPConnection::set_socket_timeout( uint64_t timeout_ms )
{
  if ( fd < 0 ) {
    return;
  }

  struct timeval tv;
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = ( timeout_ms % 1000 ) * 1000;

  /* Set receive timeout */
  if ( setsockopt( fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof( tv ) ) < 0 ) {
    if ( verbose > 0 ) {
      fprintf( stderr, "[TCP] Warning: could not set SO_RCVTIMEO: %s\n", strerror( errno ) );
    }
  }

  /* Set send timeout */
  if ( setsockopt( fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof( tv ) ) < 0 ) {
    if ( verbose > 0 ) {
      fprintf( stderr, "[TCP] Warning: could not set SO_SNDTIMEO: %s\n", strerror( errno ) );
    }
  }
}

/* Reconnect after connection loss (client mode only) - non-blocking version.
 *
 * Crypto safety across reconnect:
 *
 * The Crypto::Session object is intentionally NOT reset on reconnect.
 * This is safe because:
 *
 * 1. Session::encrypt()/decrypt() are stateless with respect to nonces.
 *    OCB mode uses the nonce provided in each Message directly; there is
 *    no internal "expected next nonce" state in the Session object.
 *
 * 2. Nonce uniqueness is guaranteed by Crypto::unique(), a process-global
 *    monotonically increasing counter (static in crypto.cc).  It survives
 *    reconnect because it is not tied to any connection object.  After
 *    reconnect, the sender simply continues with higher nonce values.
 *
 * 3. The recv_buffer and send_buffer are cleared on reconnect, discarding
 *    any partially-received ciphertext from the old TCP stream.  This
 *    prevents framing corruption (a partial length-prefix from the old
 *    connection being combined with data from the new connection).
 *
 * 4. blocks_encrypted tracks cumulative blocks for the OCB 2^47 safety
 *    limit.  It correctly persists across reconnect since the same key
 *    is in use.
 *
 * Compare with UDP: UDP is connectionless and handles "reconnection"
 * implicitly -- packets simply arrive from a new source address.  The
 * same Crypto::Session object is used throughout, with the same
 * process-global nonce counter.  TCP reconnect is analogous.
 */
void TCPConnection::reconnect( void )
{
  if ( server ) {
    /* Server doesn't reconnect */
    return;
  }

  /* Initialize reconnection state on first call */
  if ( !reconnecting ) {
    close_connection();
    reconnecting = true;
    reconnect_attempt = 0;
    next_reconnect_time = frozen_timestamp(); /* Try immediately first time */

    if ( verbose > 0 ) {
      fprintf( stderr, "[TCP] Connection lost, attempting to reconnect...\n" );
    }
  }

  /* Check if it's time to attempt reconnection */
  uint64_t now = frozen_timestamp();
  if ( now < next_reconnect_time ) {
    /* Too soon, return and let caller retry later */
    return;
  }

  /* Make a single connection attempt */
  try {
    connect_with_timeout( remote_addr, CONNECT_TIMEOUT );

    /* Success! */
    if ( verbose > 0 ) {
      fprintf( stderr, "[TCP] Reconnected successfully after %u attempt(s)\n", reconnect_attempt + 1 );
    }

    /* Clear buffers to discard partial framing data from the old TCP stream.
     * Crypto::Session persists intentionally -- see comment block above. */
    recv_buffer.clear();
    send_buffer.clear();

    /* Reset reconnection state */
    reconnecting = false;
    reconnect_attempt = 0;
    next_reconnect_time = 0;

    return;
  } catch ( const NetworkException& e ) {
    if ( verbose > 1 ) {
      fprintf( stderr, "[TCP] Reconnect attempt %u failed: %s\n", reconnect_attempt + 1, e.what() );
    }

    /* Calculate next retry time with exponential backoff (max 5 seconds) */
    reconnect_attempt++;
    int delay = RECONNECT_DELAY * ( 1 << ( reconnect_attempt < 5 ? reconnect_attempt : 5 ) );
    if ( delay > 5000 ) {
      delay = 5000;
    }

    next_reconnect_time = now + delay;

    if ( verbose > 1 ) {
      fprintf( stderr, "[TCP] Will retry in %d ms\n", delay );
    }

    /* Return immediately - caller will retry later */
    return;
  }
}

/* Flush queued outgoing data (non-blocking) */
void TCPConnection::flush_send_buffer( void )
{
  while ( !send_buffer.empty() ) {
    /* Use send() with MSG_NOSIGNAL on Linux to prevent SIGPIPE when the
       peer has closed the connection.  On macOS/BSD, SO_NOSIGPIPE is set
       on the socket (see setup_socket_options), so plain write() is safe. */
#ifdef MSG_NOSIGNAL
    ssize_t n = ::send( fd, send_buffer.data(), send_buffer.size(), MSG_NOSIGNAL );
#else
    ssize_t n = write( fd, send_buffer.data(), send_buffer.size() );
#endif
    if ( n > 0 ) {
      send_buffer.erase( 0, n );
    } else if ( n < 0 ) {
      if ( errno == EINTR ) {
        continue;
      }
      if ( errno == EAGAIN || errno == EWOULDBLOCK ) {
        return; /* Kernel buffer full, try later */
      }
      throw NetworkException( "write", errno );
    }
  }
}

/* Create a packet with payload */
Packet TCPConnection::new_packet( const std::string& s_payload )
{
  uint16_t outgoing_timestamp_reply = -1;

  uint64_t now = timestamp();

  if ( now - saved_timestamp_received_at < 1000 ) { /* recent timestamp */
    /* send "corrected" timestamp advanced by how long we held it */
    outgoing_timestamp_reply = saved_timestamp + ( now - saved_timestamp_received_at );
    saved_timestamp = -1;
    saved_timestamp_received_at = 0;
  }

  Packet p( direction, timestamp16(), outgoing_timestamp_reply, s_payload );

  return p;
}

/* Update RTT estimates */
void TCPConnection::update_rtt( uint16_t timestamp_reply )
{
  if ( timestamp_reply == uint16_t( -1 ) || saved_timestamp_received_at == uint64_t( -1 ) ) {
    return;
  }

  uint16_t now = timestamp16();
  double R = timestamp_diff( now, timestamp_reply );

  if ( R < 0 ) {
    return; /* Ignore negative RTT */
  }

  if ( !RTT_hit ) {
    SRTT = R;
    RTTVAR = R / 2.0;
    RTT_hit = true;
  } else {
    const double alpha = 1.0 / 8.0;
    const double beta = 1.0 / 4.0;

    RTTVAR = ( 1 - beta ) * RTTVAR + beta * fabs( SRTT - R );
    SRTT = ( 1 - alpha ) * SRTT + alpha * R;
  }
}

/* Send a message */
void TCPConnection::send( const std::string& s )
{
  /* Client: if reconnecting, try to reconnect */
  if ( !server && reconnecting ) {
    reconnect();
    /* If still not connected, fall through to error handling below */
  }

  if ( !connected ) {
    if ( server ) {
      /* Server not connected yet - store error but don't throw */
      send_error = "Not connected";
      return;
    } else {
      /* Client not connected - initiate reconnection */
      reconnect();
      send_error = "Connection lost, reconnecting...";
      return;
    }
  }

  try {
    /* Create packet */
    Packet p = new_packet( s );
    Message m = p.toMessage();

    /* Encrypt */
    std::string encrypted = session.encrypt( m );

    /* Check size before conversion to uint32_t */
    size_t encrypted_size = encrypted.size();
    if ( encrypted_size > MAX_MESSAGE_SIZE ) {
      throw NetworkException( "message too large", E2BIG );
    }

    /* Append length-prefixed message to send buffer */
    uint32_t len = static_cast<uint32_t>( encrypted_size );
    uint32_t net_len = htonl( len );
    send_buffer.append( reinterpret_cast<const char*>( &net_len ), sizeof( net_len ) );
    send_buffer.append( encrypted );

    /* Try to drain the buffer immediately */
    flush_send_buffer();

    send_error.clear();

    if ( verbose > 2 ) {
      fprintf( stderr, "[TCP] Sent message: %u bytes\n", len );
    }
  } catch ( const NetworkException& e ) {
    send_error = e.what();
    if ( !server ) {
      /* Client - try to reconnect and retry */
      reconnect();
      /* After reconnection, the caller will retry */
      throw;
    } else {
      /* Server - connection lost, close data fd and wait for client to reconnect */
      close_connection();
      recv_buffer.clear();
      send_buffer.clear();
      /* Don't throw - let the main loop continue and accept a new connection */
      return;
    }
  }
}

/* Receive one complete message (non-blocking) */
std::string TCPConnection::recv_one( void )
{
  /* Validate fd before use */
  if ( fd < 0 ) {
    throw NetworkException( "invalid file descriptor", EBADF );
  }

  /* Attempt one non-blocking read to gather whatever data is available */
  char buf[4096];
  ssize_t n = read( fd, buf, sizeof( buf ) );
  if ( n > 0 ) {
    /* Protect against unbounded buffer growth */
    if ( recv_buffer.size() + static_cast<size_t>( n ) > MAX_MESSAGE_SIZE + sizeof( uint32_t ) ) {
      throw NetworkException( "receive buffer overflow", E2BIG );
    }
    recv_buffer.append( buf, n );
  } else if ( n == 0 ) {
    throw NetworkException( "read: connection closed", 0 );
  } else {
    if ( errno == EINTR ) {
      /* Interrupted, no data this time - fall through to buffer check */
    } else if ( errno == EAGAIN || errno == EWOULDBLOCK ) {
      /* No data available right now - fall through to buffer check */
    } else {
      throw NetworkException( "read", errno );
    }
  }

  /* Check if recv_buffer contains a complete message */
  if ( recv_buffer.size() < sizeof( uint32_t ) ) {
    return std::string(); /* Not enough data for length prefix yet */
  }

  uint32_t net_len;
  memcpy( &net_len, recv_buffer.data(), sizeof( net_len ) );
  uint32_t len = ntohl( net_len );

  if ( len > MAX_MESSAGE_SIZE ) {
    throw NetworkException( "received message too large", E2BIG );
  }

  if ( recv_buffer.size() < sizeof( uint32_t ) + len ) {
    return std::string(); /* Incomplete message body, return and try later */
  }

  /* Extract complete message */
  std::string encrypted = recv_buffer.substr( sizeof( uint32_t ), len );
  recv_buffer.erase( 0, sizeof( uint32_t ) + len );

  /* Decrypt -- uses the nonce embedded in the ciphertext.
   * Crypto::Session has no internal nonce state; decryption succeeds as long as
   * the key and nonce match what the sender used.  Nonce gaps (from messages
   * lost during the old TCP connection) do not cause decryption failure. */
  Message m = session.decrypt( encrypted );
  Packet p( m );

  /* Verify direction bit to prevent playback of our own messages */
  if ( p.direction != ( server ? TO_SERVER : TO_CLIENT ) ) {
    throw NetworkException( "received message with wrong direction", 0 );
  }

  /* Update RTT */
  update_rtt( p.timestamp_reply );

  /* Save timestamp for echo */
  saved_timestamp = p.timestamp;
  saved_timestamp_received_at = timestamp();

  /* Track sequence for application-level replay awareness.  TCP's ordered
   * reliable delivery means we should never see out-of-order packets in
   * normal operation.  After reconnect, the sender's nonce counter will
   * have advanced (Crypto::unique() is process-global), so p.seq will be
   * higher than expected_receiver_seq -- this is expected and harmless. */
  expected_receiver_seq = p.seq + 1;

  /* Update last heard */
  last_heard = timestamp();

  if ( verbose > 2 ) {
    fprintf( stderr, "[TCP] Received message: %u bytes\n", len );
  }

  return p.payload;
}

/* Receive a message */
std::string TCPConnection::recv( void )
{
  /* Opportunistically drain pending writes on every event loop iteration */
  if ( connected && !send_buffer.empty() ) {
    try {
      flush_send_buffer();
    } catch ( const NetworkException& e ) {
      if ( verbose > 0 ) {
        fprintf( stderr, "[TCP] flush_send_buffer error: %s\n", e.what() );
      }
      if ( !server ) {
        reconnect();
      } else {
        close_connection();
        recv_buffer.clear();
        send_buffer.clear();
      }
      return std::string();
    }
  }

  /* Server: accept connection if not connected yet */
  if ( server && !connected ) {
    accept_connection();
    if ( !connected ) {
      /* No client yet */
      return std::string();
    }
  }

  /* Client: if reconnecting, try to reconnect */
  if ( !server && reconnecting ) {
    reconnect();
    /* If still not connected, return empty string */
    if ( !connected ) {
      return std::string();
    }
  }

  if ( !connected ) {
    return std::string();
  }

  try {
    return recv_one();
  } catch ( const NetworkException& e ) {
    if ( verbose > 0 ) {
      fprintf( stderr, "[TCP] recv error: %s\n", e.what() );
    }
    if ( !server ) {
      /* Client - initiate reconnection (non-blocking) */
      reconnect();
    } else {
      /* Server - connection lost, close data fd and wait for client to reconnect */
      close_connection();
      recv_buffer.clear();
    }
    return std::string();
  }
}

/* Get file descriptors for select/poll */
const std::vector<int> TCPConnection::fds( void ) const
{
  std::vector<int> result;
  if ( server && listen_fd >= 0 ) {
    /* Always include listen_fd so we can detect new connection attempts */
    result.push_back( listen_fd );
  }
  if ( fd >= 0 ) {
    result.push_back( fd );
  }
  return result;
}

/* Get timeout */
uint64_t TCPConnection::timeout( void ) const
{
  /* Calculate RTO like UDP */
  uint64_t RTO = (uint64_t)lrint( ceil( SRTT + 4 * RTTVAR ) );

  if ( RTO < MIN_TCP_TIMEOUT ) {
    RTO = MIN_TCP_TIMEOUT;
  }
  if ( RTO > MAX_TCP_TIMEOUT ) {
    RTO = MAX_TCP_TIMEOUT;
  }

  return RTO;
}

/* Get port */
std::string TCPConnection::port( void ) const
{
  if ( server ) {
    return bind_port;
  }

  /* Client: get local port from socket */
  if ( fd < 0 ) {
    return std::string();
  }

  Addr local_addr;
  socklen_t local_addr_len = sizeof( local_addr );
  if ( getsockname( fd, &local_addr.sa, &local_addr_len ) < 0 ) {
    return std::string();
  }

  char port_str[16];
  if ( local_addr.sa.sa_family == AF_INET ) {
    snprintf( port_str, sizeof( port_str ), "%d", ntohs( local_addr.sin.sin_port ) );
  } else if ( local_addr.sa.sa_family == AF_INET6 ) {
    snprintf( port_str, sizeof( port_str ), "%d", ntohs( local_addr.sin6.sin6_port ) );
  } else {
    return std::string();
  }

  return std::string( port_str );
}

/* Set timeout */
void TCPConnection::set_timeout( uint64_t ms )
{
  if ( ms < MIN_TCP_TIMEOUT ) {
    ms = MIN_TCP_TIMEOUT;
  }
  if ( ms > MAX_TCP_TIMEOUT ) {
    ms = MAX_TCP_TIMEOUT;
  }

  tcp_timeout = ms;
  set_socket_timeout( ms );
}

/* Close connection */
void TCPConnection::close_connection( void )
{
  if ( fd >= 0 ) {
    shutdown( fd, SHUT_RDWR );
    close( fd );
    fd = -1;
  }
  connected = false;
  send_buffer.clear();
}

/* Check if connected */
bool TCPConnection::is_connected( void ) const
{
  return connected;
}

/* Parse port range (same as UDP) */
bool TCPConnection::parse_portrange( const char* desired_port_range, int& desired_port_low, int& desired_port_high )
{
  /* Parse port or port range */
  if ( strchr( desired_port_range, ':' ) ) {
    /* Port range */
    if ( sscanf( desired_port_range, "%d:%d", &desired_port_low, &desired_port_high ) != 2 ) {
      return false;
    }
  } else {
    /* Single port */
    if ( sscanf( desired_port_range, "%d", &desired_port_low ) != 1 ) {
      return false;
    }
    desired_port_high = desired_port_low;
  }

  /* Validate range */
  if ( desired_port_low < 0 || desired_port_low > 65535 || desired_port_high < 0 || desired_port_high > 65535
       || desired_port_low > desired_port_high ) {
    return false;
  }

  return true;
}
