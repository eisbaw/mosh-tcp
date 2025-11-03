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
*/

/*
 * test-connection - Simple test program for network transport layer
 *
 * Usage:
 *   Server: ./test-connection server [port]
 *   Client: ./test-connection client <host> <port> <key>
 *
 * This program tests the Transport<> layer directly without SSH bootstrap.
 * It sends simple text messages back and forth using the same state
 * synchronization protocol as Mosh.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>

#include "src/network/networktransport-impl.h"

/*
 * MockState - Simple state class for testing
 *
 * Just holds a string message and can compute diffs.
 */
class MockState
{
private:
  std::string message;
  uint64_t msg_num;

public:
  MockState() : message( "" ), msg_num( 0 ) {}
  MockState( const std::string& s ) : message( s ), msg_num( 0 ) {}

  /* Get the message */
  std::string get_message() const { return message; }

  /* Set a new message */
  void set_message( const std::string& s )
  {
    message = s;
    msg_num++;
  }

  /* Compare states */
  bool operator==( const MockState& other ) const { return message == other.message; }

  bool compare( const MockState& other ) const { return !( *this == other ); }

  /* Compute diff (for state sync) - just return the new message */
  std::string diff_from( const MockState& existing ) const
  {
    if ( message == existing.message ) {
      return "";
    }
    return message;
  }

  /* Apply diff */
  void apply_string( const std::string& diff )
  {
    if ( !diff.empty() ) {
      message = diff;
      msg_num++;
    }
  }

  /* For transport initialization */
  std::string init_diff() const { return message; }

  /* Reset input flag (required by Transport) */
  void reset_input() {}

  /* Subtitle for display */
  std::string subtitle() const { return ""; }
};

void print_usage( const char* progname )
{
  fprintf( stderr, "Usage:\n" );
  fprintf( stderr, "  Server: %s server [port]\n", progname );
  fprintf( stderr, "  Client: %s client <host> <port> <key>\n", progname );
  fprintf( stderr, "\n" );
  fprintf( stderr, "Example:\n" );
  fprintf( stderr, "  # Terminal 1 (server):\n" );
  fprintf( stderr, "  %s server 60001\n", progname );
  fprintf( stderr, "\n" );
  fprintf( stderr, "  # Terminal 2 (client):\n" );
  fprintf( stderr, "  %s client localhost 60001 <key-from-server>\n", progname );
  fprintf( stderr, "\n" );
}

int run_server( const char* port )
{
  printf( "Starting test server on port %s...\n", port );

  try {
    /* Create initial states */
    MockState local_state( "Server: Ready" );
    MockState remote_state;

    /* Create server transport */
    using TestTransport = Network::Transport<MockState, MockState>;
    TestTransport transport( local_state, remote_state, nullptr, port );

    printf( "Server listening on port %s\n", transport.port().c_str() );
    printf( "Connection key: %s\n", transport.get_key().c_str() );
    printf( "\nWaiting for client connection...\n" );
    printf( "Client command: %s client localhost %s %s\n\n", program_invocation_name, transport.port().c_str(),
            transport.get_key().c_str() );

    /* Main loop */
    int count = 0;
    while ( true ) {
      /* Wait for events */
      int wait_ms = transport.wait_time();

      fd_set read_fds;
      FD_ZERO( &read_fds );
      std::vector<int> fds = transport.fds();
      int max_fd = -1;
      for ( int fd : fds ) {
        FD_SET( fd, &read_fds );
        if ( fd > max_fd )
          max_fd = fd;
      }

      struct timeval tv;
      tv.tv_sec = wait_ms / 1000;
      tv.tv_usec = ( wait_ms % 1000 ) * 1000;

      int ret = select( max_fd + 1, &read_fds, nullptr, nullptr, &tv );

      if ( ret < 0 ) {
        perror( "select" );
        return 1;
      }

      /* Send any pending data */
      transport.tick();

      /* Receive if data available */
      if ( ret > 0 ) {
        transport.recv();

        /* Check for new remote state */
        std::string remote_msg = transport.get_latest_remote_state().state.get_message();
        if ( !remote_msg.empty() ) {
          printf( "Received: %s\n", remote_msg.c_str() );

          /* Send response */
          count++;
          char response[256];
          snprintf( response, sizeof( response ), "Server: Got message #%d", count );
          transport.get_current_state().set_message( response );
        }
      }

      /* Check for shutdown */
      if ( transport.shutdown_acknowledged() ) {
        printf( "Client disconnected.\n" );
        break;
      }
    }

    printf( "Server exiting.\n" );
    return 0;

  } catch ( const Network::NetworkException& e ) {
    fprintf( stderr, "Network error: %s (errno=%d)\n", e.function.c_str(), e.the_errno );
    return 1;
  } catch ( const std::exception& e ) {
    fprintf( stderr, "Error: %s\n", e.what() );
    return 1;
  }
}

int run_client( const char* host, const char* port, const char* key )
{
  printf( "Connecting to %s:%s...\n", host, port );

  try {
    /* Create initial states */
    MockState local_state;
    MockState remote_state;

    /* Create client transport */
    using TestTransport = Network::Transport<MockState, MockState>;
    TestTransport transport( local_state, remote_state, key, host, port );

    printf( "Connected!\n\n" );

    /* Send test messages */
    const char* messages[] = { "Hello from client", "Testing state sync", "Message three", "Final message",
                               nullptr };

    for ( int i = 0; messages[i] != nullptr; i++ ) {
      printf( "Sending: %s\n", messages[i] );
      transport.get_current_state().set_message( messages[i] );

      /* Give time for round-trip */
      for ( int j = 0; j < 10; j++ ) {
        transport.tick();

        int wait_ms = transport.wait_time();
        if ( wait_ms > 100 )
          wait_ms = 100;

        fd_set read_fds;
        FD_ZERO( &read_fds );
        std::vector<int> fds = transport.fds();
        int max_fd = -1;
        for ( int fd : fds ) {
          FD_SET( fd, &read_fds );
          if ( fd > max_fd )
            max_fd = fd;
        }

        struct timeval tv;
        tv.tv_sec = wait_ms / 1000;
        tv.tv_usec = ( wait_ms % 1000 ) * 1000;

        int ret = select( max_fd + 1, &read_fds, nullptr, nullptr, &tv );
        if ( ret < 0 ) {
          perror( "select" );
          return 1;
        }

        if ( ret > 0 ) {
          transport.recv();

          /* Check for response */
          std::string remote_msg = transport.get_latest_remote_state().state.get_message();
          if ( !remote_msg.empty() ) {
            printf( "  <- %s\n", remote_msg.c_str() );
          }
        }
      }

      sleep( 1 );
    }

    /* Shutdown gracefully */
    printf( "\nShutting down...\n" );
    transport.start_shutdown();

    /* Wait for shutdown acknowledgment */
    for ( int i = 0; i < 50; i++ ) {
      transport.tick();

      int wait_ms = transport.wait_time();
      if ( wait_ms > 100 )
        wait_ms = 100;

      fd_set read_fds;
      FD_ZERO( &read_fds );
      std::vector<int> fds = transport.fds();
      int max_fd = -1;
      for ( int fd : fds ) {
        FD_SET( fd, &read_fds );
        if ( fd > max_fd )
          max_fd = fd;
      }

      struct timeval tv;
      tv.tv_sec = wait_ms / 1000;
      tv.tv_usec = ( wait_ms % 1000 ) * 1000;

      select( max_fd + 1, &read_fds, nullptr, nullptr, &tv );
      transport.recv();

      if ( transport.shutdown_acknowledged() ) {
        printf( "Shutdown acknowledged.\n" );
        break;
      }
    }

    printf( "Client exiting.\n" );
    return 0;

  } catch ( const Network::NetworkException& e ) {
    fprintf( stderr, "Network error: %s (errno=%d)\n", e.function.c_str(), e.the_errno );
    return 1;
  } catch ( const std::exception& e ) {
    fprintf( stderr, "Error: %s\n", e.what() );
    return 1;
  }
}

int main( int argc, char* argv[] )
{
  if ( argc < 2 ) {
    print_usage( argv[0] );
    return 1;
  }

  if ( strcmp( argv[1], "server" ) == 0 ) {
    const char* port = ( argc > 2 ) ? argv[2] : "0"; /* 0 = pick random port */
    return run_server( port );
  } else if ( strcmp( argv[1], "client" ) == 0 ) {
    if ( argc < 5 ) {
      fprintf( stderr, "Error: Client requires host, port, and key arguments.\n\n" );
      print_usage( argv[0] );
      return 1;
    }
    return run_client( argv[2], argv[3], argv[4] );
  } else {
    fprintf( stderr, "Error: Unknown mode '%s'\n\n", argv[1] );
    print_usage( argv[0] );
    return 1;
  }
}
