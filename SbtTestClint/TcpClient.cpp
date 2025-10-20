#include "TcpClient.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

const int TcpClient::BUFFER_SIZE;

TcpClient::TcpClient(const std::string& server_ip, int port)
    : server_ip_(server_ip), port_(port), client_socket_(-1) {
    memset(&server_addr_, 0, sizeof(server_addr_));
    init_address_struct();
}

TcpClient::~TcpClient() {
    close_connection();
}

void TcpClient::init_address_struct() {
    server_addr_.sin_family = AF_INET;
    server_addr_.sin_port = htons(port_);

    if (inet_pton(AF_INET, server_ip_.c_str(), &server_addr_.sin_addr) <= 0) {
        std::cerr << "Warning: invalid server address - will be checked during connection" << std::endl;
    }
}

bool TcpClient::connect_to_server() {
    if (client_socket_ != -1) {
        close_connection();
    }

    if ((client_socket_ = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Error: socket creation failed" << std::endl;
        return false;
    }

    if (connect(client_socket_, (struct sockaddr*)&server_addr_, sizeof(server_addr_)) < 0) {
        std::cerr << "Error: connection to server failed" << std::endl;
        close_connection();
        return false;
    }

    std::cout << "Connected to server " << server_ip_ << ":" << port_ << std::endl;
    return true;
}

bool TcpClient::send_command(const std::string& cmd, const std::string& param2, std::string& result) {
    if (client_socket_ == -1) {
        std::cerr << "Error: not connected to server" << std::endl;
        return false;
    }

    std::string send_data = cmd + "|||" + param2;

    ssize_t total_sent = 0;
    while (total_sent < send_data.size()) {
        ssize_t sent = send(client_socket_, send_data.c_str() + total_sent,
                           send_data.size() - total_sent, 0);
        if (sent < 0) {
            std::cerr << "Error: sending command failed" << std::endl;
            return false;
        }
        total_sent += sent;
    }

    std::cout << "Command sent to server" << std::endl;

    result.clear();
    char buffer[BUFFER_SIZE] = {0};
    ssize_t valread;

    while ((valread = read(client_socket_, buffer, BUFFER_SIZE - 1)) > 0) {
        buffer[valread] = '\0';
        result += buffer;
        memset(buffer, 0, BUFFER_SIZE);
    }

    if (valread < 0) {
        std::cerr << "Error: reading from socket failed" << std::endl;
        return false;
    }

    return true;
}

void TcpClient::close_connection() {
    if (client_socket_ != -1) {
        close(client_socket_);
        client_socket_ = -1;
        std::cout << "Connection closed" << std::endl;
    }
}
