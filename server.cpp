#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

constexpr size_t k_max_msg = 32 << 20; // 32MB

void die(const char* msg) {
    std::cerr << msg << std::endl;
    WSACleanup();
    exit(1);
}

int read_full(SOCKET sock, char* buf, size_t n) {
    while (n > 0) {
        int rv = recv(sock, buf, static_cast<int>(n), 0);
        if (rv <= 0) return -1;
        buf += rv;
        n -= rv;
    }
    return 0;
}

int write_all(SOCKET sock, const char* buf, size_t n) {
    while (n > 0) {
        int rv = send(sock, buf, static_cast<int>(n), 0);
        if (rv <= 0) return -1;
        buf += rv;
        n -= rv;
    }
    return 0;
}

int handle_req(const std::string& req, std::string& res) {
    res.clear();
    for (char c : req) {
        res.push_back(toupper(static_cast<unsigned char>(c)));
    }
    return 0;
}

void server_loop(SOCKET client_sock) {
    while (true) {
        char len_buf[4];
        if (read_full(client_sock, len_buf, 4) != 0) break;

        int32_t len = ntohl(*reinterpret_cast<int32_t*>(len_buf));
        if (len <= 0 || len > k_max_msg) break;

        std::vector<char> req_buf(len);
        if (read_full(client_sock, req_buf.data(), len) != 0) break;

        std::string req(req_buf.begin(), req_buf.end());
        std::string res;
        if (handle_req(req, res) != 0) break;

        int32_t res_len = static_cast<int32_t>(res.size());
        int32_t res_len_net = htonl(res_len);

        if (write_all(client_sock, reinterpret_cast<const char*>(&res_len_net), 4) != 0) break;
        if (write_all(client_sock, res.data(), res.size()) != 0) break;
    }

    closesocket(client_sock);
    std::cout << "Client disconnected.\n";
}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        die("WSAStartup failed");
    }

    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == INVALID_SOCKET) {
        die("socket() failed");
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(listen_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        die("bind() failed");
    }

    if (listen(listen_sock, SOMAXCONN) == SOCKET_ERROR) {
        die("listen() failed");
    }

    std::cout << "Server listening on port 1234...\n";

    while (true) {
        SOCKET client_sock = accept(listen_sock, nullptr, nullptr);
        if (client_sock == INVALID_SOCKET) {
            std::cerr << "accept() failed\n";
            continue;
        }

        std::cout << "Client connected!\n";
        server_loop(client_sock);
    }

    closesocket(listen_sock); // Proper cleanup of the listening socket
    WSACleanup();
    return 0;
}
