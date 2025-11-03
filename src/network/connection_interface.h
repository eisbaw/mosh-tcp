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

#ifndef CONNECTION_INTERFACE_HPP
#define CONNECTION_INTERFACE_HPP

#include <cstdint>
#include <string>
#include <vector>

#include <netinet/in.h>
#include <sys/socket.h>

namespace Network {

/* Forward declaration of Addr union from network.h */
union Addr;

/**
 * Abstract interface for network connections.
 *
 * This interface abstracts the underlying transport protocol (UDP, TCP, etc.)
 * from the higher-level state synchronization protocol. Implementations must
 * provide encrypted, bi-directional communication with the remote peer.
 *
 * All implementations must:
 * - Handle encryption/decryption of payloads
 * - Track round-trip time for congestion control
 * - Provide file descriptors for select/poll
 * - Report appropriate MTU for the transport
 */
class ConnectionInterface
{
public:
  virtual ~ConnectionInterface() {}

  /**
   * Send an encrypted message to the remote peer.
   *
   * @param s The payload to send (will be encrypted by implementation)
   * @throws NetworkException on fatal errors
   */
  virtual void send( const std::string& s ) = 0;

  /**
   * Receive an encrypted message from the remote peer.
   *
   * @return The decrypted payload, or empty string if no data available
   * @throws NetworkException on fatal errors
   */
  virtual std::string recv( void ) = 0;

  /**
   * Get file descriptors to monitor for I/O readiness.
   *
   * @return Vector of file descriptors for use with select/poll
   */
  virtual const std::vector<int> fds( void ) const = 0;

  /**
   * Get current timeout value for retransmission.
   *
   * @return Timeout in milliseconds
   */
  virtual uint64_t timeout( void ) const = 0;

  /**
   * Get Maximum Transmission Unit for this connection.
   *
   * @return MTU in bytes (application payload size)
   */
  virtual int get_MTU( void ) const = 0;

  /**
   * Get local port number as a string.
   *
   * @return Port number (e.g., "60001")
   */
  virtual std::string port( void ) const = 0;

  /**
   * Get encryption key as a printable string.
   *
   * @return Base64-encoded encryption key
   */
  virtual std::string get_key( void ) const = 0;

  /**
   * Check if remote address is known.
   *
   * @return true if we have received data from remote peer
   */
  virtual bool get_has_remote_addr( void ) const = 0;

  /**
   * Get smoothed round-trip time estimate.
   *
   * @return SRTT in milliseconds
   */
  virtual double get_SRTT( void ) const = 0;

  /**
   * Notify connection of successful round-trip.
   *
   * Called by transport layer when an acknowledgment is received.
   *
   * @param timestamp_in Time of successful round-trip
   */
  virtual void set_last_roundtrip_success( uint64_t timestamp_in ) = 0;

  /**
   * Get last send error message.
   *
   * @return Reference to error string (may be empty)
   */
  virtual std::string& get_send_error( void ) = 0;

  /**
   * Get remote peer address.
   *
   * @return Reference to remote address structure
   */
  virtual const Addr& get_remote_addr( void ) const = 0;

  /**
   * Get length of remote address structure.
   *
   * @return Size of remote address in bytes
   */
  virtual socklen_t get_remote_addr_len( void ) const = 0;
};

} // namespace Network

#endif /* CONNECTION_INTERFACE_HPP */
