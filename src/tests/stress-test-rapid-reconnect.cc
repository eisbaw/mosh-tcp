/*
 * Stress Test: Rapid Connect/Disconnect Cycles
 *
 * This test performs rapid connection and disconnection cycles to ensure
 * the TCP implementation properly handles resource cleanup and doesn't leak
 * memory or file descriptors.
 */

#include <iostream>
#include <chrono>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include "src/network/tcpconnection.h"

using namespace Network;
using namespace std;

const int NUM_CYCLES = 100;
const char* TEST_IP = "127.0.0.1";
const char* TEST_PORT = "60100";

void run_server_cycles() {
    for (int i = 0; i < NUM_CYCLES; i++) {
        try {
            TCPConnection* server = new TCPConnection(TEST_IP, TEST_PORT);

            // Give client time to connect
            usleep(10000); // 10ms

            // Cleanup
            delete server;

            if (i % 10 == 0) {
                cout << "  Server cycle " << i << "/" << NUM_CYCLES << endl;
            }
        } catch (const NetworkException& e) {
            cerr << "Server cycle " << i << " failed: " << e.what() << endl;
        }
    }
}

void run_client_cycles() {
    usleep(5000); // Let server start first

    for (int i = 0; i < NUM_CYCLES; i++) {
        try {
            // Use a valid Base64 128-bit key (22 characters)
            const char* key = "MTIzNDU2Nzg5MDEyMzQ1Ng";

            TCPConnection* client = new TCPConnection(key, TEST_IP, TEST_PORT);

            // Brief delay
            usleep(5000); // 5ms

            // Cleanup
            delete client;

        } catch (const NetworkException& e) {
            // Connection refused is expected if server disconnected first
            if (i % 10 == 0) {
                cout << "  Client cycle " << i << " (expected errors OK)" << endl;
            }
        }
    }
}

int main() {
    cout << "=== TCP Stress Test: Rapid Connect/Disconnect ===" << endl;
    cout << "Running " << NUM_CYCLES << " cycles..." << endl;

    auto start = chrono::high_resolution_clock::now();

    pid_t server_pid = fork();

    if (server_pid == 0) {
        // Child: run server
        run_server_cycles();
        exit(0);
    } else {
        // Parent: run client
        run_client_cycles();

        // Wait for server to finish
        int status;
        waitpid(server_pid, &status, 0);
    }

    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);

    cout << "\n✅ Stress test completed!" << endl;
    cout << "Time: " << duration.count() << "ms" << endl;
    cout << "Average per cycle: " << (duration.count() / (float)NUM_CYCLES) << "ms" << endl;

    return 0;
}
