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

#ifndef TCPCONNECTION_HPP
#define TCPCONNECTION_HPP

#include <cstdint>
#include <string>
#include <vector>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include "connection_interface.h"
#include "network.h"
#include "src/crypto/crypto.h"

using namespace Crypto;

namespace Network {

/*
 * TCP-based connection implementation.
 *
 * This class implements the ConnectionInterface using TCP streams with
 * length-prefixed message framing. Features automatic reconnection on
 * connection loss (client mode) and aggressive timeout configuration.
 */
class TCPConnection : public ConnectionInterface
{
private:
  /* TCP timeout configuration */
  static const uint64_t DEFAULT_TCP_TIMEOUT = 500;  /* ms */
  static const uint64_t MIN_TCP_TIMEOUT = 100;      /* ms */
  static const uint64_t MAX_TCP_TIMEOUT = 1000;     /* ms */
  static const uint64_t CONNECT_TIMEOUT = 1000;     /* ms */
  static const int RECONNECT_DELAY = 100;           /* ms between reconnect attempts */

  /* TCP MTU - larger than UDP since kernel handles segmentation */
  static const int DEFAULT_TCP_MTU = 8192;

  /* Maximum message size (prevent memory exhaustion) */
  static const uint32_t MAX_MESSAGE_SIZE = 1048576; /* 1 MB */

  /* Port range for server binding */
  static const int PORT_RANGE_LOW = 60001;
  static const int PORT_RANGE_HIGH = 60999;

  /* Socket file descriptor */
  int fd;
  int listen_fd; /* Separate listening socket for server mode */

  /* Connection state */
  bool server;    /* Server or client mode */
  bool connected; /* Connection established */

  /* Network addresses */
  Addr remote_addr;        /* Remote peer address */
  socklen_t remote_addr_len;
  bool has_remote_addr;
  std::string bind_ip;     /* Server: bind address */
  std::string bind_port;   /* Server: bind port */

  /* Protocol parameters */
  int MTU;                 /* Application MTU */
  uint64_t tcp_timeout;    /* Current timeout value */

  /* Encryption */
  Base64Key key;
  Session session;

  /* RTT tracking (application-level, like UDP) */
  Direction direction;
  uint16_t saved_timestamp;
  uint64_t saved_timestamp_received_at;
  uint64_t expected_receiver_seq;
  bool RTT_hit;
  double SRTT;
  double RTTVAR;

  /* Connection monitoring */
  uint64_t last_heard;
  uint64_t last_roundtrip_success;

  /* Error reporting */
  std::string send_error;

  /* Message framing buffer */
  std::string recv_buffer; /* Accumulates partial messages */

  /* Verbosity */
  unsigned int verbose;

  /* Private helper methods */

  /* Socket setup and configuration */
  void setup( void );
  void setup_socket_options( void );
  void set_socket_timeout( uint64_t timeout_ms );

  /* Server socket methods */
  void bind_and_listen( const char* ip, const char* port );
  void accept_connection( void ); /* Accept client connection */

  /* Client connection methods */
  void connect_with_timeout( const Addr& addr, uint64_t timeout_ms );
  void reconnect( void ); /* Reconnect after connection loss */

  /* I/O helpers */
  ssize_t read_fully( void* buf, size_t len );
  ssize_t write_fully( const void* buf, size_t len );

  /* Message framing */
  std::string recv_one( void ); /* Receive one complete message */

  /* Packet creation */
  Packet new_packet( const std::string& s_payload );

  /* RTT update */
  void update_rtt( uint16_t timestamp_reply );

  /* Connection management */
  void close_connection( void );
  bool is_connected( void ) const;

public:
  /* Network transport overhead: 4 bytes length + standard overhead */
  static const int ADDED_BYTES = 4 /* length prefix */ + 8 /* seqno/nonce */ + 4 /* timestamps */;

  /* Constructors */
  TCPConnection( const char* desired_ip, const char* desired_port );      /* server */
  TCPConnection( const char* key_str, const char* ip, const char* port ); /* client */

  /* Destructor */
  ~TCPConnection();

  /* Disable copy */
  TCPConnection( const TCPConnection& ) = delete;
  TCPConnection& operator=( const TCPConnection& ) = delete;

  /* ConnectionInterface implementation */
  void send( const std::string& s ) override;
  std::string recv( void ) override;
  const std::vector<int> fds( void ) const override;
  uint64_t timeout( void ) const override;
  int get_MTU( void ) const override { return MTU; }

  std::string port( void ) const override;
  std::string get_key( void ) const override { return key.printable_key(); }
  bool get_has_remote_addr( void ) const override { return has_remote_addr; }

  double get_SRTT( void ) const override { return SRTT; }
  void set_last_roundtrip_success( uint64_t s_success ) override { last_roundtrip_success = s_success; }

  const Addr& get_remote_addr( void ) const override { return remote_addr; }
  socklen_t get_remote_addr_len( void ) const override { return remote_addr_len; }

  std::string& get_send_error( void ) override { return send_error; }

  /* Configuration methods */
  void set_timeout( uint64_t ms );
  void set_verbose( unsigned int v ) { verbose = v; }

  /* Static utility methods */
  static bool parse_portrange( const char* desired_port_range, int& desired_port_low, int& desired_port_high );
};

} // namespace Network

#endif /* TCPCONNECTION_HPP */
