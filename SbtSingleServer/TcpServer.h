#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctime>
#include <vector>

class TcpServer {
private:
    int port_;               // 服务端口
    int server_fd_;          // 服务器socket描述符
    struct sockaddr_in address_;  // 地址结构
    static const int MAX_CMD_LENGTH = 1024 * 10;  // 最大命令长度限制
    static const int BUFFER_SIZE = 4096 * 100;     // 缓冲区大小

    // 处理子进程退出
    static void handle_sigchld(int sig);

    // 执行命令并返回结果
    std::string execute_command(const std::string& received_data);

    // 处理客户端连接
    void handle_client(int client_socket);

    std::string get_current_time_string();

public:
    // 构造函数
    TcpServer(int port = 8888);

    // 析构函数
    ~TcpServer();

    // 初始化服务器
    bool init();

    // 启动服务器，进入监听循环
    void start();

    // 停止服务器
    void stop();
};

#endif // TCP_SERVER_H
