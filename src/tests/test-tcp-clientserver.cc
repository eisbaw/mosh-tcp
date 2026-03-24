/*
    TCP Client-Server Communication Test

    Tests multiple message exchanges between TCP client and server,
    verifying echo replies match expectations.
    Uses a pipe to coordinate key and port -- no temp files, no hardcoded ports.
*/

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#include "src/network/tcpconnection.h"

using namespace Network;
using namespace std;

static const int NUM_MESSAGES = 3;

static void run_server( int write_fd )
{
  try {
    TCPConnection server( "127.0.0.1", "0" );
    server.set_verbose( 0 );

    /* Send port and key to parent via pipe */
    string info = server.port() + "\n" + server.get_key() + "\n";
    ssize_t written = write( write_fd, info.c_str(), info.size() );
    close( write_fd );
    if ( written < 0 ) {
      cerr << "[Server] Failed to write to pipe" << endl;
      _exit( 1 );
    }

    cerr << "[Server] Listening on port " << server.port() << endl;

    /* Receive and echo messages */
    int messages_received = 0;
    int attempts = 0;
    while ( messages_received < NUM_MESSAGES && attempts < 100 ) {
      string msg = server.recv();
      if ( !msg.empty() ) {
        messages_received++;
        cerr << "[Server] Received #" << messages_received << ": '" << msg << "'" << endl;

        string reply = "SERVER_ECHO:" + msg;
        server.send( reply );
        cerr << "[Server] Sent reply: '" << reply << "'" << endl;
      }
      usleep( 50000 ); /* 50ms */
      attempts++;
    }

    cerr << "[Server] Received " << messages_received << "/" << NUM_MESSAGES << " messages" << endl;

    if ( messages_received >= NUM_MESSAGES ) {
      /* Give client time to receive last reply */
      usleep( 500000 );
      _exit( 0 );
    } else {
      _exit( 1 );
    }

  } catch ( const exception& e ) {
    cerr << "[Server] ERROR: " << e.what() << endl;
    _exit( 1 );
  }
}

static int run_client( const string& port, const string& key )
{
  try {
    cerr << "[Client] Connecting to 127.0.0.1:" << port << endl;

    TCPConnection client( key.c_str(), "127.0.0.1", port.c_str() );
    client.set_verbose( 0 );

    cerr << "[Client] Connected" << endl;

    int replies_received = 0;

    for ( int i = 0; i < NUM_MESSAGES; i++ ) {
      string msg = "TestMessage_" + to_string( i );
      cerr << "[Client] Sending: '" << msg << "'" << endl;
      client.send( msg );

      /* Wait for reply */
      string reply;
      for ( int retry = 0; retry < 50; retry++ ) {
        reply = client.recv();
        if ( !reply.empty() ) {
          break;
        }
        usleep( 50000 ); /* 50ms */
      }

      if ( !reply.empty() ) {
        replies_received++;
        cerr << "[Client] Reply #" << replies_received << ": '" << reply << "'" << endl;

        string expected = "SERVER_ECHO:" + msg;
        if ( reply != expected ) {
          cerr << "[Client] MISMATCH: expected '" << expected << "'" << endl;
          return 1;
        }
      } else {
        cerr << "[Client] No reply for message " << i << endl;
      }
    }

    cerr << "[Client] Received " << replies_received << "/" << NUM_MESSAGES << " replies" << endl;

    return ( replies_received >= NUM_MESSAGES ) ? 0 : 1;

  } catch ( const exception& e ) {
    cerr << "[Client] ERROR: " << e.what() << endl;
    return 1;
  }
}

int main()
{
  cerr << "=== TCP Client-Server Communication Test ===" << endl;

  int pipefd[2];
  if ( pipe( pipefd ) < 0 ) {
    cerr << "pipe() failed: " << strerror( errno ) << endl;
    return 1;
  }

  pid_t server_pid = fork();
  if ( server_pid < 0 ) {
    cerr << "fork() failed" << endl;
    return 1;
  }

  if ( server_pid == 0 ) {
    /* Child: server */
    close( pipefd[0] );
    run_server( pipefd[1] );
    _exit( 1 ); /* should not reach here */
  }

  /* Parent: client */
  close( pipefd[1] );

  /* Read port and key from server */
  char buf[512];
  ssize_t n = read( pipefd[0], buf, sizeof( buf ) - 1 );
  close( pipefd[0] );

  if ( n <= 0 ) {
    cerr << "Failed to read port/key from server" << endl;
    kill( server_pid, SIGTERM );
    wait( NULL );
    return 1;
  }
  buf[n] = '\0';

  string data( buf );
  size_t nl = data.find( '\n' );
  if ( nl == string::npos ) {
    cerr << "Malformed server info" << endl;
    kill( server_pid, SIGTERM );
    wait( NULL );
    return 1;
  }

  string port = data.substr( 0, nl );
  string key = data.substr( nl + 1 );
  if ( !key.empty() && key.back() == '\n' ) {
    key.pop_back();
  }

  int client_result = run_client( port, key );

  /* Wait for server */
  int status;
  waitpid( server_pid, &status, 0 );

  bool server_ok = WIFEXITED( status ) && WEXITSTATUS( status ) == 0;

  if ( client_result == 0 && server_ok ) {
    cerr << "\nPASS: TCP client-server test" << endl;
    return 0;
  }

  cerr << "\nFAIL: TCP client-server test"
       << " (client=" << ( client_result == 0 ? "ok" : "fail" )
       << ", server=" << ( server_ok ? "ok" : "fail" ) << ")" << endl;
  return 1;
}
