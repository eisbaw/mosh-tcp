/*
 * Fuzz Test: TCP Message Parser
 *
 * This test sends malformed, truncated, and malicious data to the TCP
 * message parser to verify robustness and crash resistance.
 */

#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "src/network/tcpconnection.h"

using namespace Network;
using namespace std;

const char* TEST_IP = "127.0.0.1";
const char* TEST_PORT = "60102";

// Generate various malformed inputs
vector<string> generate_fuzz_inputs() {
    vector<string> inputs;

    // 1. Invalid length prefix (too large)
    string invalid_len(4, '\xFF');
    inputs.push_back(invalid_len);

    // 2. Zero length
    uint32_t zero_len = 0;
    string zero_msg((char*)&zero_len, 4);
    inputs.push_back(zero_msg);

    // 3. Truncated message (length says 100, but only send 10)
    uint32_t len = htonl(100);
    string truncated((char*)&len, 4);
    truncated += string(10, 'X');
    inputs.push_back(truncated);

    // 4. Multiple partial length prefixes
    inputs.push_back(string(1, '\x00'));
    inputs.push_back(string(2, '\x00'));
    inputs.push_back(string(3, '\x00'));

    // 5. Very small length
    len = htonl(1);
    string tiny((char*)&len, 4);
    tiny += "X";
    inputs.push_back(tiny);

    // 6. Maximum valid length (64KB)
    len = htonl(65536);
    string maxsize((char*)&len, 4);
    maxsize += string(65536, 'M');
    inputs.push_back(maxsize);

    // 7. Length that would overflow (if not validated)
    len = htonl(0xFFFFFFFF);
    string overflow((char*)&len, 4);
    inputs.push_back(overflow);

    // 8. Random garbage
    inputs.push_back(string(100, '\xAA'));
    inputs.push_back(string(100, '\x00'));

    // 9. Mixed valid and invalid
    len = htonl(10);
    string mixed((char*)&len, 4);
    mixed += string(10, 'V'); // Valid message
    mixed += string(4, '\xFF'); // Invalid length prefix
    inputs.push_back(mixed);

    return inputs;
}

void run_server() {
    try {
        TCPConnection server(TEST_IP, TEST_PORT);
        cout << "Fuzz server: Ready" << endl;

        // Try to receive messages (most will fail/timeout)
        for (int i = 0; i < 20; i++) {
            try {
                string msg = server.recv();
                if (!msg.empty()) {
                    cout << "  Received valid message: " << msg.size() << " bytes" << endl;
                }
            } catch (const NetworkException& e) {
                // Expected for malformed input
                cout << "  Exception (expected): " << e.what() << endl;
            }
            usleep(10000); // 10ms
        }

        cout << "Fuzz server: Completed without crash!" << endl;

    } catch (const NetworkException& e) {
        cout << "Server error: " << e.what() << endl;
    }
}

void send_raw_data(const string& data, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, TEST_IP, &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        send(sock, data.data(), data.size(), 0);
    }

    close(sock);
}

void run_fuzzer() {
    usleep(100000); // Wait for server

    vector<string> fuzz_inputs = generate_fuzz_inputs();

    cout << "Fuzzer: Sending " << fuzz_inputs.size() << " malformed inputs..." << endl;

    for (size_t i = 0; i < fuzz_inputs.size(); i++) {
        cout << "  Sending fuzz input #" << i+1 << " (" << fuzz_inputs[i].size() << " bytes)" << endl;
        send_raw_data(fuzz_inputs[i], atoi(TEST_PORT));
        usleep(100000); // 100ms between attempts
    }

    cout << "Fuzzer: All inputs sent" << endl;
}

int main() {
    cout << "=== TCP Fuzz Test: Message Parser ===" << endl;
    cout << "Testing robustness against malformed input..." << endl << endl;

    pid_t pid = fork();

    if (pid == 0) {
        // Child: server
        run_server();
        exit(0);
    } else {
        // Parent: fuzzer
        run_fuzzer();

        // Wait for server
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            cout << "\n✅ Fuzz test completed - no crashes!" << endl;
            return 0;
        } else if (WIFSIGNALED(status)) {
            cout << "\n❌ Server crashed with signal " << WTERMSIG(status) << endl;
            return 1;
        }
    }

    return 0;
}
