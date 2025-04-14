#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <vector>

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

int32_t send_req(SOCKET sock, const char* data, size_t len) {
    if (len > k_max_msg) return -1;

    int32_t len_net = htonl(static_cast<int32_t>(len));
    if (write_all(sock, reinterpret_cast<const char*>(&len_net), 4)) return -1;
    if (write_all(sock, data, len)) return -1;

    return 0;
}

int32_t read_res(SOCKET sock) {
    char buf[4];
    if (read_full(sock, buf, 4)) return -1;

    int32_t len = ntohl(*reinterpret_cast<int32_t*>(buf));
    if (len > k_max_msg) return -1;

    std::vector<char> data(len);
    if (read_full(sock, data.data(), len)) return -1;

    std::string res(data.begin(), data.end());
    std::cout << "Server response: " << res << std::endl;

    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: client <host> <port>" << std::endl;
        return 1;
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        die("WSAStartup failed");
    }

    struct addrinfo hints = {};
    struct addrinfo* res = nullptr;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(argv[1], argv[2], &hints, &res) != 0) {
        die("getaddrinfo failed");
    }

    SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == INVALID_SOCKET) {
        die("socket creation failed");
    }

    if (connect(sock, res->ai_addr, static_cast<int>(res->ai_addrlen)) != 0) {
        closesocket(sock);
        die("connect failed");
    }

    freeaddrinfo(res);

    std::string msg;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, msg);
        if (msg.empty()) break;

        if (send_req(sock, msg.c_str(), msg.size()) != 0) {
            std::cerr << "send failed" << std::endl;
            break;
        }

        if (read_res(sock) != 0) {
            std::cerr << "read failed" << std::endl;
            break;
        }
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
