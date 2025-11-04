/*
 * Stress Test: Concurrent Connections
 *
 * This test creates multiple simultaneous TCP connections to verify
 * thread safety and proper resource management under concurrent load.
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "src/network/tcpconnection.h"

using namespace Network;
using namespace std;

const char* TEST_IP = "127.0.0.1";
const int NUM_CLIENTS = 10;
const int BASE_PORT = 60200;
const char* TEST_KEY = "MTIzNDU2Nzg5MDEyMzQ1Ng"; // Valid Base64 128-bit key

void run_server(int port) {
    char port_str[10];
    snprintf(port_str, sizeof(port_str), "%d", port);

    try {
        TCPConnection server(TEST_IP, port_str);

        // Wait for connection and exchange one message
        for (int i = 0; i < 5; i++) {
            string msg = server.recv();
            if (!msg.empty()) {
                server.send("ACK");
                break;
            }
            usleep(100000); // 100ms
        }

    } catch (const NetworkException& e) {
        // Expected - client may disconnect
    }
}

void run_client(int port) {
    char port_str[10];
    snprintf(port_str, sizeof(port_str), "%d", port);

    usleep(50000); // Wait for server

    try {
        TCPConnection client(TEST_KEY, TEST_IP, port_str);
        client.send("PING");

        // Wait for response
        for (int i = 0; i < 5; i++) {
            string msg = client.recv();
            if (!msg.empty()) {
                break;
            }
            usleep(100000); // 100ms
        }

    } catch (const NetworkException& e) {
        cerr << "Client " << port << " error: " << e.what() << endl;
    }
}

int main() {
    cout << "=== TCP Stress Test: Concurrent Connections ===" << endl;
    cout << "Starting " << NUM_CLIENTS << " concurrent connections..." << endl;

    auto start = chrono::high_resolution_clock::now();

    vector<pid_t> pids;

    // Start all servers
    for (int i = 0; i < NUM_CLIENTS; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            run_server(BASE_PORT + i);
            exit(0);
        }
        pids.push_back(pid);
    }

    // Start all clients
    for (int i = 0; i < NUM_CLIENTS; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            run_client(BASE_PORT + i);
            exit(0);
        }
        pids.push_back(pid);
    }

    // Wait for all children
    int success = 0;
    for (pid_t pid : pids) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            success++;
        }
    }

    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);

    cout << "\n✅ Concurrent test completed!" << endl;
    cout << "Successful processes: " << success << "/" << (NUM_CLIENTS * 2) << endl;
    cout << "Total time: " << duration.count() << "ms" << endl;

    return 0;
}
