#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>

class TcpClient {
private:
    std::string server_ip_;
    int port_;
    int client_socket_;
    struct sockaddr_in server_addr_;
    static const int BUFFER_SIZE = 4096 * 100;
    void init_address_struct();

public:
    TcpClient(const std::string& server_ip, int port);
    ~TcpClient();
    bool connect_to_server();
    bool send_command(const std::string& cmd, const std::string& param2, std::string& result);
    void close_connection();
};

#endif // TCP_CLIENT_H
