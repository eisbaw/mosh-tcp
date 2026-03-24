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
 * test-connection - Test the Transport<> layer over TCP
 *
 * When run without arguments (make check mode):
 *   Forks a server and client, exchanges messages via Transport<MockState>,
 *   and verifies state synchronization works.
 *
 * When run with arguments (manual mode):
 *   Server: ./test-connection server [port]
 *   Client: ./test-connection client <host> <port> <key>
 */

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

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

  /* Subtract common prefix (required by TransportSender) */
  void subtract( const MockState* ) { /* No-op for simple string state */ }
};

using TestTransport = Network::Transport<MockState, MockState>;

/* Helper: run select loop for a transport, processing events for up to max_iterations */
static void pump( TestTransport& transport, int max_iterations, int iter_ms )
{
  for ( int i = 0; i < max_iterations; i++ ) {
    int wait_ms = transport.wait_time();
    if ( wait_ms > iter_ms ) {
      wait_ms = iter_ms;
    }

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
    if ( ret < 0 && errno != EINTR ) {
      return;
    }

    transport.tick();

    if ( ret > 0 ) {
      transport.recv();
    }
  }
}

/* ============================================================
 * Automated test mode (no arguments)
 * ============================================================ */

static void auto_test_server( int write_fd )
{
  try {
    MockState local_state( "Server:Ready" );
    MockState remote_state;

    TestTransport transport( local_state, remote_state, nullptr, "0" );

    /* Send port and key to parent via pipe */
    std::string info = transport.port() + "\n" + transport.get_key() + "\n";
    ssize_t written = write( write_fd, info.c_str(), info.size() );
    close( write_fd );
    if ( written < 0 ) {
      _exit( 1 );
    }

    fprintf( stderr, "[Server] Listening on port %s\n", transport.port().c_str() );

    /* Run the event loop, waiting for client messages */
    int count = 0;
    for ( int i = 0; i < 200 && count < 2; i++ ) {
      pump( transport, 1, 100 );

      std::string remote_msg = transport.get_latest_remote_state().state.get_message();
      if ( !remote_msg.empty() && remote_msg.find( "Client:" ) == 0 ) {
        count++;
        fprintf( stderr, "[Server] Got: %s\n", remote_msg.c_str() );

        char response[256];
        snprintf( response, sizeof( response ), "Server:Ack:%d", count );
        transport.get_current_state().set_message( response );
      }

      if ( transport.shutdown_acknowledged() ) {
        break;
      }
    }

    fprintf( stderr, "[Server] Received %d client messages\n", count );
    _exit( count >= 2 ? 0 : 1 );

  } catch ( const std::exception& e ) {
    fprintf( stderr, "[Server] ERROR: %s\n", e.what() );
    _exit( 1 );
  }
}

static int auto_test_client( const std::string& port, const std::string& key )
{
  try {
    MockState local_state;
    MockState remote_state;

    fprintf( stderr, "[Client] Connecting to 127.0.0.1:%s\n", port.c_str() );

    TestTransport transport( local_state, remote_state, key.c_str(), "127.0.0.1", port.c_str() );

    fprintf( stderr, "[Client] Connected\n" );

    /* Send two messages, check for server acks */
    const char* messages[] = { "Client:Hello", "Client:World" };
    int acks = 0;

    for ( int m = 0; m < 2; m++ ) {
      transport.get_current_state().set_message( messages[m] );
      fprintf( stderr, "[Client] Sent: %s\n", messages[m] );

      /* Pump and look for ack */
      for ( int i = 0; i < 50; i++ ) {
        pump( transport, 1, 100 );

        std::string remote_msg = transport.get_latest_remote_state().state.get_message();
        if ( !remote_msg.empty() && remote_msg.find( "Server:Ack:" ) == 0 ) {
          fprintf( stderr, "[Client] Got ack: %s\n", remote_msg.c_str() );
          acks++;
          break;
        }
      }
    }

    /* Shutdown */
    transport.start_shutdown();
    pump( transport, 20, 100 );

    fprintf( stderr, "[Client] Received %d acks\n", acks );
    return ( acks >= 2 ) ? 0 : 1;

  } catch ( const std::exception& e ) {
    fprintf( stderr, "[Client] ERROR: %s\n", e.what() );
    return 1;
  }
}

static int run_auto_test()
{
  fprintf( stderr, "=== Transport Connection Test (automated) ===\n" );

  int pipefd[2];
  if ( pipe( pipefd ) < 0 ) {
    fprintf( stderr, "pipe() failed: %s\n", strerror( errno ) );
    return 1;
  }

  pid_t pid = fork();
  if ( pid < 0 ) {
    fprintf( stderr, "fork() failed\n" );
    return 1;
  }

  if ( pid == 0 ) {
    /* Child: server */
    close( pipefd[0] );
    auto_test_server( pipefd[1] );
    _exit( 1 );
  }

  /* Parent: client */
  close( pipefd[1] );

  char buf[512];
  ssize_t n = read( pipefd[0], buf, sizeof( buf ) - 1 );
  close( pipefd[0] );

  if ( n <= 0 ) {
    fprintf( stderr, "Failed to read port/key from server\n" );
    kill( pid, SIGTERM );
    wait( NULL );
    return 1;
  }
  buf[n] = '\0';

  std::string data( buf );
  size_t nl = data.find( '\n' );
  if ( nl == std::string::npos ) {
    fprintf( stderr, "Malformed server info\n" );
    kill( pid, SIGTERM );
    wait( NULL );
    return 1;
  }

  std::string port = data.substr( 0, nl );
  std::string key = data.substr( nl + 1 );
  if ( !key.empty() && key.back() == '\n' ) {
    key.pop_back();
  }

  int client_result = auto_test_client( port, key );

  int status;
  waitpid( pid, &status, 0 );
  bool server_ok = WIFEXITED( status ) && WEXITSTATUS( status ) == 0;

  if ( client_result == 0 && server_ok ) {
    fprintf( stderr, "\nPASS: Transport connection test\n" );
    return 0;
  }

  fprintf( stderr, "\nFAIL: Transport connection test (client=%s, server=%s)\n",
           client_result == 0 ? "ok" : "fail", server_ok ? "ok" : "fail" );
  return 1;
}

/* ============================================================
 * Manual mode (with arguments)
 * ============================================================ */

static void print_usage( const char* progname )
{
  fprintf( stderr, "Usage:\n" );
  fprintf( stderr, "  Automated: %s\n", progname );
  fprintf( stderr, "  Server:    %s server [port]\n", progname );
  fprintf( stderr, "  Client:    %s client <host> <port> <key>\n", progname );
}

static int run_server( const char* port )
{
  printf( "Starting test server on port %s...\n", port );
  fflush( stdout );

  try {
    MockState local_state( "Server: Ready" );
    MockState remote_state;

    TestTransport transport( local_state, remote_state, nullptr, port );

    printf( "Server listening on port %s\n", transport.port().c_str() );
    printf( "Connection key: %s\n", transport.get_key().c_str() );
    printf( "Client command: test-connection client localhost %s %s\n\n",
            transport.port().c_str(), transport.get_key().c_str() );
    fflush( stdout );

    int count = 0;
    while ( true ) {
      pump( transport, 1, 250 );

      std::string remote_msg = transport.get_latest_remote_state().state.get_message();
      if ( !remote_msg.empty() ) {
        printf( "Received: %s\n", remote_msg.c_str() );
        count++;
        char response[256];
        snprintf( response, sizeof( response ), "Server: Got message #%d", count );
        transport.get_current_state().set_message( response );
      }

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

static int run_client( const char* host, const char* port, const char* key )
{
  printf( "Connecting to %s:%s...\n", host, port );

  try {
    MockState local_state;
    MockState remote_state;

    TestTransport transport( local_state, remote_state, key, host, port );

    printf( "Connected!\n\n" );

    const char* messages[] = { "Hello from client", "Testing state sync", "Message three", "Final message",
                               nullptr };

    for ( int i = 0; messages[i] != nullptr; i++ ) {
      printf( "Sending: %s\n", messages[i] );
      transport.get_current_state().set_message( messages[i] );

      /* Give time for round-trip */
      for ( int j = 0; j < 20; j++ ) {
        pump( transport, 1, 100 );

        std::string remote_msg = transport.get_latest_remote_state().state.get_message();
        if ( !remote_msg.empty() ) {
          printf( "  <- %s\n", remote_msg.c_str() );
          break;
        }
      }
    }

    /* Shutdown gracefully */
    printf( "\nShutting down...\n" );
    transport.start_shutdown();
    pump( transport, 50, 100 );

    if ( transport.shutdown_acknowledged() ) {
      printf( "Shutdown acknowledged.\n" );
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
    /* No arguments: run automated test */
    return run_auto_test();
  }

  if ( strcmp( argv[1], "server" ) == 0 ) {
    const char* port = ( argc > 2 ) ? argv[2] : "0";
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
