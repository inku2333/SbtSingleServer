#include <iostream>
#include "TcpClient.h"

int main(int argc, char const* argv[]) {

    std::string server_ip = "192.168.1.250";
    int port = 8888;
    std::string cmd = "ps -ef";
    std::string para2 = "Test";

    TcpClient client(server_ip, port);

    if (!client.connect_to_server()) {
        return 1;
    }

    std::string result;
    if (client.send_command(cmd, para2, result)) {
        std::cout << "Command execution result:\n" << result << std::endl;
    } else {
        std::cerr << "Failed to execute command on server" << std::endl;
    }

    return 0;
}
