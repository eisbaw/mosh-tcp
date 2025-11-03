/*
    TCP Client-Server Communication Test

    Tests actual message exchange between TCP client and server.
*/

#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>
#include <sys/wait.h>

#include "src/network/tcpconnection.h"

using namespace Network;
using namespace std;

#define KEY_FILE "/tmp/mosh-tcp-test-key.txt"

void run_server() {
    try {
        cerr << "[Server] Creating TCP server on port 60052..." << endl;

        TCPConnection server("127.0.0.1", "60052");
        server.set_verbose(1);

        // Write key to file for client
        string key = server.get_key();
        ofstream keyfile(KEY_FILE);
        keyfile << key << endl;
        keyfile.close();

        cerr << "[Server] Listening on port " << server.port() << endl;
        cerr << "[Server] Key saved to " << KEY_FILE << endl;

        // Receive and echo messages
        int messages_received = 0;
        for (int i = 0; i < 10 && messages_received < 3; i++) {
            cerr << "[Server] Waiting for message (attempt " << i << ")..." << endl;
            string msg = server.recv();

            if (!msg.empty()) {
                messages_received++;
                cerr << "[Server] ✓ Received message #" << messages_received
                     << ": '" << msg << "' (" << msg.size() << " bytes)" << endl;

                // Echo back with prefix
                string reply = "SERVER_ECHO:" + msg;
                server.send(reply);
                cerr << "[Server] ✓ Sent reply: '" << reply << "'" << endl;
            }

            sleep(1);
        }

        cerr << "[Server] Test complete. Received " << messages_received << " messages" << endl;

        if (messages_received >= 3) {
            cerr << "[Server] ✅ SUCCESS" << endl;
            exit(0);
        } else {
            cerr << "[Server] ❌ FAILED - not enough messages" << endl;
            exit(1);
        }

    } catch (const exception& e) {
        cerr << "[Server] ❌ ERROR: " << e.what() << endl;
        exit(1);
    }
}

void run_client() {
    try {
        // Wait for server to start and write key
        cerr << "[Client] Waiting for server to start..." << endl;
        sleep(3);

        // Read key from file
        ifstream keyfile(KEY_FILE);
        if (!keyfile.is_open()) {
            cerr << "[Client] ❌ ERROR: Cannot open key file" << endl;
            exit(1);
        }

        string key;
        getline(keyfile, key);
        keyfile.close();

        cerr << "[Client] Connecting to 127.0.0.1:60052..." << endl;
        cerr << "[Client] Using key: " << key << endl;

        TCPConnection client(key.c_str(), "127.0.0.1", "60052");
        client.set_verbose(1);

        cerr << "[Client] ✓ Connected!" << endl;

        // Send test messages and wait for replies
        int replies_received = 0;
        for (int i = 0; i < 3; i++) {
            string msg = "TestMessage_" + to_string(i);
            cerr << "[Client] Sending: '" << msg << "'" << endl;
            client.send(msg);

            // Wait for reply
            for (int retry = 0; retry < 5; retry++) {
                sleep(1);
                string reply = client.recv();
                if (!reply.empty()) {
                    replies_received++;
                    cerr << "[Client] ✓ Received reply #" << (i+1)
                         << ": '" << reply << "'" << endl;

                    // Verify it's an echo
                    string expected = "SERVER_ECHO:" + msg;
                    if (reply == expected) {
                        cerr << "[Client] ✓ Reply matches expected echo" << endl;
                    } else {
                        cerr << "[Client] ⚠ Reply mismatch! Expected: '" << expected << "'" << endl;
                    }
                    break;
                }
            }
        }

        cerr << "[Client] Test complete. Received " << replies_received << " replies" << endl;

        if (replies_received >= 3) {
            cerr << "[Client] ✅ SUCCESS" << endl;
            exit(0);
        } else {
            cerr << "[Client] ❌ FAILED - not enough replies" << endl;
            exit(1);
        }

    } catch (const exception& e) {
        cerr << "[Client] ❌ ERROR: " << e.what() << endl;
        exit(1);
    }
}

int main() {
    cerr << "=== TCP Client-Server Communication Test ===" << endl;

    // Clean up old key file
    unlink(KEY_FILE);

    pid_t server_pid = fork();
    if (server_pid < 0) {
        cerr << "Fork failed" << endl;
        return 1;
    }

    if (server_pid == 0) {
        // Child: run server
        run_server();
        return 0;
    }

    // Parent: run client
    run_client();

    // Wait for server to finish
    int status;
    waitpid(server_pid, &status, 0);

    // Clean up
    unlink(KEY_FILE);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        cerr << "\n✅ TCP CLIENT-SERVER TEST PASSED!" << endl;
        return 0;
    } else {
        cerr << "\n❌ TCP CLIENT-SERVER TEST FAILED!" << endl;
        return 1;
    }
}
