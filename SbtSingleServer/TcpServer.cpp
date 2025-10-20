#include "TcpServer.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <cstdio>
#include <sys/wait.h>
#include <csignal>
#include <sys/select.h>

// 静态成员初始化
const int TcpServer::MAX_CMD_LENGTH;
const int TcpServer::BUFFER_SIZE;

// 信号处理函数，处理子进程退出
void TcpServer::handle_sigchld(int sig) {
    (void)sig;
    // 处理所有已退出的子进程
    while (waitpid(-1, nullptr, WNOHANG) > 0);
}

// 构造函数
TcpServer::TcpServer(int port) : port_(port), server_fd_(-1) {
    memset(&address_, 0, sizeof(address_));
}

// 析构函数
TcpServer::~TcpServer() {
    stop();
}

// 初始化服务器
bool TcpServer::init() {
    // 设置信号处理
    struct sigaction sa;
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;  // 重启被信号中断的系统调用
    if (sigaction(SIGCHLD, &sa, nullptr) == -1) {
        std::cerr << "Error: sigaction failed" << std::endl;
        return false;
    }

    // 创建socket
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "Error: socket creation failed" << std::endl;
        return false;
    }

    // 设置socket选项，允许端口重用
    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        std::cerr << "Error: setsockopt failed" << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    // 设置地址结构
    address_.sin_family = AF_INET;
    address_.sin_addr.s_addr = INADDR_ANY;  // 监听所有网络接口
    address_.sin_port = htons(port_);

    // 绑定端口
    if (bind(server_fd_, (struct sockaddr*)&address_, sizeof(address_)) < 0) {
        std::cerr << "Error: bind failed" << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    // 开始监听
    if (listen(server_fd_, 5) < 0) {
        std::cerr << "Error: listen failed" << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    std::cout << "Server initialized, listening on port " << port_ << std::endl;
    return true;
}

// 添加时间字符串生成函数
std::string TcpServer::get_current_time_string() {
    time_t now = time(nullptr);
    struct tm tstruct;
    char buf[80];
    tstruct = *localtime(&now);
    // 格式: 年-月-日_时-分-秒
    strftime(buf, sizeof(buf), "%Y-%m-%d_%H:%M:%S", &tstruct);
    return std::string(buf);
}

std::string TcpServer::execute_command(const std::string& received_data) {
    // 分割接收到的数据，分离命令和第二个参数
    size_t sep_pos = received_data.find("|||");
    if (sep_pos == std::string::npos) {
        return "Error: Invalid data format. Missing separator.";
    }

    std::string cmd = received_data.substr(0, sep_pos);
    std::string param2 = received_data.substr(sep_pos + 3);  // 跳过分隔符

    // 获取当前时间字符串
    std::string timestamp = get_current_time_string();
    std::string full_timestamp = get_current_time_string();  // 带毫秒的时间戳

    // 日志文件名：参数2_sysCmd_当前时间.log
    std::string log_filename = param2 + "_sysCmd_" + timestamp + ".log";

    // 1. 先写入日志头部信息（使用echo命令重定向）
    std::string header_cmd = "echo \"Command executed: " + cmd + "\" > " + log_filename + "\n";
    header_cmd += "echo \"Execution start time: " + full_timestamp + "\" >> " + log_filename + "\n";
    header_cmd += "echo \"=============================================\" >> " + log_filename + "\n";
    header_cmd += "echo \"Command output:\" >> " + log_filename + "\n";
    header_cmd += "echo \"=============================================\" >> " + log_filename;

    // 执行头部写入命令
    system(header_cmd.c_str());

    // 2. 执行原始用户命令，并通过重定向将所有输出（stdout和stderr）追加到日志文件
    std::string full_cmd = cmd + " >> " + log_filename + " 2>&1";
    int exit_status = system(full_cmd.c_str());

    // 3. 写入日志尾部信息
    std::string footer_cmd = "echo \"=============================================\" >> " + log_filename + "\n";
    footer_cmd += "echo \"Execution end time: " + get_current_time_string() + "\" >> " + log_filename + "\n";
    footer_cmd += "echo \"Exit status: " + std::to_string(WEXITSTATUS(exit_status)) + "\" >> " + log_filename;
    system(footer_cmd.c_str());

    // 返回日志文件名和退出状态
    return "Command executed. Log file: " + log_filename +
            "\nExit status: " + std::to_string(WEXITSTATUS(exit_status));
}

// 处理客户端连接
void TcpServer::handle_client(int client_socket) {
    char buffer[MAX_CMD_LENGTH + 1] = {0};

    // 读取客户端发送的命令，限制最大长度
    ssize_t bytes_read = read(client_socket, buffer, MAX_CMD_LENGTH);
    if (bytes_read < 0) {
        std::cerr << "Error reading from socket" << std::endl;
        close(client_socket);
        return;
    }
    else if (bytes_read == 0) {
        std::cout << "Client closed connection" << std::endl;
        close(client_socket);
        return;
    }

    std::string cmd(buffer, bytes_read);
    std::cout << "Received command: " << cmd << std::endl;

    // 执行命令
    std::string result = execute_command(cmd);

    // 发送结果回客户端
    ssize_t total_sent = 0;
    while (total_sent < result.size()) {
        ssize_t sent = send(client_socket, result.c_str() + total_sent,
                            result.size() - total_sent, 0);
        if (sent < 0) {
            std::cerr << "Error sending data to client" << std::endl;
            close(client_socket);
            return;
        }
        total_sent += sent;
    }

    std::cout << "Sent result to client" << std::endl;

    // 关闭连接
    close(client_socket);
}

// 启动服务器，进入监听循环
void TcpServer::start() {
    if (server_fd_ < 0) {
        std::cerr << "Error: Server not initialized" << std::endl;
        return;
    }

    std::cout << "Server started, waiting for connections..." << std::endl;

    std::vector<int> client_fds;
    fd_set readfds;       // select读集合 bitmap
    int max_fd = server_fd_;  // 最大文件描述符，用于select

    while (true) {
        // 清空并重新设置select集合
        FD_ZERO(&readfds);
        // 监听服务器socket（接受新连接）
        FD_SET(server_fd_, &readfds);

        // 监听所有已连接的客户端socket
        for (int fd : client_fds) {
            FD_SET(fd, &readfds);
            if (fd > max_fd) {
                max_fd = fd;  // 更新最大文件描述符
            }
        }

        // 等待活动（阻塞，无超时）
        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0 && errno != EINTR) {
            std::cerr << "Error: select failed" << std::endl;
            break;
        }

        // 处理新连接（服务器socket有活动）
        if (FD_ISSET(server_fd_, &readfds)) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int new_socket = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
            if (new_socket < 0) {
                std::cerr << "Error: accept failed" << std::endl;
                continue;
            }

            // 打印新连接信息
            std::cout << "New connection from "
                      << inet_ntoa(client_addr.sin_addr)
                      << ":" << ntohs(client_addr.sin_port)
                      << " (socket: " << new_socket << ")" << std::endl;

            // 将新连接加入客户端列表
            client_fds.push_back(new_socket);
        }

        // 处理客户端数据（遍历所有客户端socket）
        for (auto it = client_fds.begin(); it != client_fds.end(); ) {
            int client_socket = *it;
            if (FD_ISSET(client_socket, &readfds)) {
                // 创建子进程处理请求
                pid_t pid = fork();
                if (pid < 0) {
                    std::cerr << "Error: fork failed" << std::endl;
                    close(client_socket);
                    it = client_fds.erase(it);  // 移除无效连接
                }
                else if (pid == 0) {
                    // 子进程：处理客户端请求
                    close(server_fd_);  // 子进程不需要服务器socket
                    handle_client(client_socket);
                    exit(0);  // 处理完毕退出
                }
                else {
                    // 父进程：关闭客户端socket，从列表移除
                    close(client_socket);
                    it = client_fds.erase(it);
                }
            } else {
                ++it;  // 无活动，继续下一个
            }
        }
    }
}

// 停止服务器
void TcpServer::stop() {
    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
        std::cout << "Server stopped" << std::endl;
    }
}

