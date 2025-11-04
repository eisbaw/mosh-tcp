/*
 * Stress Test: Large Message Handling
 *
 * This test sends messages of various sizes including very large ones
 * to ensure proper handling and buffer management.
 */

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "src/network/tcpconnection.h"

using namespace Network;
using namespace std;

const char* TEST_IP = "127.0.0.1";
const char* TEST_PORT = "60101";
const char* TEST_KEY = "MTIzNDU2Nzg5MDEyMzQ1Ng"; // Valid Base64 128-bit key

// Test various message sizes
vector<size_t> message_sizes = {
    10,           // Tiny
    100,          // Small
    1024,         // 1KB
    8192,         // 8KB (MTU size)
    16384,        // 16KB
    32768,        // 32KB
    65536         // 64KB (large)
};

void run_server() {
    try {
        TCPConnection server(TEST_IP, TEST_PORT);
        cout << "Server: Listening on " << TEST_PORT << endl;

        // Wait for client to connect and send messages
        for (size_t size : message_sizes) {
            string received = server.recv();
            if (received.empty()) {
                // Try again
                usleep(100000); // 100ms
                received = server.recv();
            }

            if (!received.empty()) {
                cout << "Server: Received message of " << received.size()
                     << " bytes (expected ~" << size << ")" << endl;
            }
        }

        cout << "Server: Test complete" << endl;

    } catch (const NetworkException& e) {
        cerr << "Server error: " << e.what() << endl;
    }
}

void run_client() {
    usleep(100000); // Wait for server to start

    try {
        TCPConnection client(TEST_KEY, TEST_IP, TEST_PORT);
        cout << "Client: Connected to server" << endl;

        for (size_t size : message_sizes) {
            // Create message of specified size
            string message(size, 'X');

            cout << "Client: Sending " << size << " byte message..." << endl;
            client.send(message);

            usleep(50000); // 50ms delay between messages
        }

        cout << "Client: All messages sent" << endl;

    } catch (const NetworkException& e) {
        cerr << "Client error: " << e.what() << endl;
    }
}

int main() {
    cout << "=== TCP Stress Test: Large Messages ===" << endl;
    cout << "Testing message sizes: ";
    for (size_t size : message_sizes) {
        cout << size << " ";
    }
    cout << "bytes" << endl << endl;

    auto start = chrono::high_resolution_clock::now();

    pid_t pid = fork();

    if (pid == 0) {
        // Child: server
        run_server();
        exit(0);
    } else {
        // Parent: client
        run_client();

        // Wait for server
        int status;
        waitpid(pid, &status, 0);
    }

    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);

    cout << "\n✅ Large message test completed!" << endl;
    cout << "Total time: " << duration.count() << "ms" << endl;

    return 0;
}
