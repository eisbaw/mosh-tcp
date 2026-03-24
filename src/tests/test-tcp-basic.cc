/*
    Test program for TCP connection functionality

    Tests basic TCP server creation and properties,
    then tests a real client-server connection with message exchange
    using a pipe to coordinate the key and port between processes.
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

static int test_server_creation()
{
  cerr << "--- Test: Server creation and properties ---" << endl;

  try {
    TCPConnection server( "127.0.0.1", "0" );
    server.set_verbose( 0 );

    string port = server.port();
    string key = server.get_key();

    if ( port.empty() || port == "0" ) {
      cerr << "FAIL: server port is empty or zero" << endl;
      return 1;
    }
    cerr << "  Port: " << port << endl;

    if ( key.empty() ) {
      cerr << "FAIL: server key is empty" << endl;
      return 1;
    }
    cerr << "  Key length: " << key.size() << endl;

    int mtu = server.get_MTU();
    if ( mtu <= 0 ) {
      cerr << "FAIL: MTU is " << mtu << endl;
      return 1;
    }
    cerr << "  MTU: " << mtu << endl;

    auto fds = server.fds();
    if ( fds.empty() ) {
      cerr << "FAIL: no file descriptors" << endl;
      return 1;
    }
    cerr << "  FDs: " << fds.size() << endl;

    uint64_t t = server.timeout();
    cerr << "  Timeout: " << t << "ms" << endl;

    cerr << "  PASS" << endl;
    return 0;

  } catch ( const exception& e ) {
    cerr << "FAIL: " << e.what() << endl;
    return 1;
  }
}

static int test_client_server_exchange()
{
  cerr << "--- Test: Client-server message exchange ---" << endl;

  /* Pipe for server to send port+key to client */
  int pipefd[2];
  if ( pipe( pipefd ) < 0 ) {
    cerr << "FAIL: pipe() failed: " << strerror( errno ) << endl;
    return 1;
  }

  pid_t pid = fork();
  if ( pid < 0 ) {
    cerr << "FAIL: fork() failed" << endl;
    return 1;
  }

  if ( pid == 0 ) {
    /* Child: server */
    close( pipefd[0] ); /* close read end */

    try {
      TCPConnection server( "127.0.0.1", "0" );
      server.set_verbose( 0 );

      /* Send port and key to parent via pipe */
      string info = server.port() + "\n" + server.get_key() + "\n";
      ssize_t written = write( pipefd[1], info.c_str(), info.size() );
      close( pipefd[1] );
      if ( written < 0 ) {
        _exit( 1 );
      }

      /* Wait for a message from client */
      int attempts = 0;
      string msg;
      while ( attempts < 50 ) {
        msg = server.recv();
        if ( !msg.empty() ) {
          break;
        }
        usleep( 100000 ); /* 100ms */
        attempts++;
      }

      if ( msg.empty() ) {
        cerr << "[Server] No message received after " << attempts << " attempts" << endl;
        _exit( 1 );
      }

      cerr << "[Server] Received: '" << msg << "'" << endl;

      /* Echo back */
      server.send( "Echo:" + msg );

      /* Give client time to receive the reply */
      usleep( 500000 );
      _exit( 0 );

    } catch ( const exception& e ) {
      cerr << "[Server] ERROR: " << e.what() << endl;
      _exit( 1 );
    }
  }

  /* Parent: client */
  close( pipefd[1] ); /* close write end */

  int result = 1;

  try {
    /* Read port and key from pipe */
    char buf[512];
    ssize_t n = read( pipefd[0], buf, sizeof( buf ) - 1 );
    close( pipefd[0] );
    if ( n <= 0 ) {
      cerr << "FAIL: could not read port/key from server" << endl;
      kill( pid, SIGTERM );
      wait( NULL );
      return 1;
    }
    buf[n] = '\0';

    string data( buf );
    size_t nl = data.find( '\n' );
    if ( nl == string::npos ) {
      cerr << "FAIL: malformed server info" << endl;
      kill( pid, SIGTERM );
      wait( NULL );
      return 1;
    }

    string port = data.substr( 0, nl );
    string key = data.substr( nl + 1 );
    /* Remove trailing newline from key */
    if ( !key.empty() && key.back() == '\n' ) {
      key.pop_back();
    }

    cerr << "[Client] Connecting to 127.0.0.1:" << port << endl;

    TCPConnection client( key.c_str(), "127.0.0.1", port.c_str() );
    client.set_verbose( 0 );

    /* Send a test message */
    client.send( "Hello" );

    /* Wait for echo reply */
    string reply;
    for ( int i = 0; i < 50; i++ ) {
      reply = client.recv();
      if ( !reply.empty() ) {
        break;
      }
      usleep( 100000 ); /* 100ms */
    }

    if ( reply == "Echo:Hello" ) {
      cerr << "[Client] Got expected reply: '" << reply << "'" << endl;
      result = 0;
    } else if ( reply.empty() ) {
      cerr << "[Client] FAIL: no reply received" << endl;
    } else {
      cerr << "[Client] FAIL: unexpected reply '" << reply << "'" << endl;
    }

  } catch ( const exception& e ) {
    cerr << "[Client] ERROR: " << e.what() << endl;
  }

  /* Wait for server child */
  int status;
  waitpid( pid, &status, 0 );

  if ( result == 0 && WIFEXITED( status ) && WEXITSTATUS( status ) == 0 ) {
    cerr << "  PASS" << endl;
    return 0;
  }

  if ( result == 0 ) {
    cerr << "  FAIL: server exited with error" << endl;
  }
  return 1;
}

int main()
{
  cerr << "=== TCP Basic Tests ===" << endl;

  int failures = 0;

  failures += test_server_creation();
  failures += test_client_server_exchange();

  cerr << "\n=== " << ( failures == 0 ? "ALL PASSED" : "FAILURES DETECTED" ) << " ===" << endl;
  return failures > 0 ? 1 : 0;
}
