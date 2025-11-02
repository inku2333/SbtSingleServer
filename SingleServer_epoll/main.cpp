#include "TcpServer.h"
#include <iostream>

int main(int argc, char const* argv[]) {
    int port = 8888; // 默认端口

    // 允许通过命令行参数指定端口
    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
            if (port < 1 || port > 65535) {
                std::cerr << "Invalid port number, using default port " << port << std::endl;
                port = 8888;
            }
        }
        catch (...) {
            std::cerr << "Invalid port argument, using default port " << port << std::endl;
        }
    }

    // 创建并初始化服务器
    TcpServer server(port);
    if (!server.init()) {
        std::cerr << "Failed to initialize server" << std::endl;
        return 1;
    }

    // 启动服务器
    server.start();

    // 正常情况下不会到达这里，除非服务器停止
    return 0;
}
