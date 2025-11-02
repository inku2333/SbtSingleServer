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
#include <sys/epoll.h>


// 信号处理函数，处理子进程退出
void TcpServer::handle_sigchld(int sig) {
    (void)sig;
    // 批量清理所有僵尸进程
    // -1 表示清理所有子进程，WNOHANG 表示 “非阻塞清理”，有僵尸进程就清理，没有就立即返回
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

    // 设为非阻塞
    if (set_nonblocking(server_fd_) < 0) {
        std::cerr << "Error: set nonblocking failed" << std::endl;
        close(server_fd_);
        server_fd_ = -1;
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

// 新增工具函数：设置socket为非阻塞
int TcpServer::set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        std::cerr << "Error: fcntl F_GETFL failed" << std::endl;
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        std::cerr << "Error: fcntl F_SETFL O_NONBLOCK failed" << std::endl;
        return -1;
    }
    return 0;
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

    int epfd = epoll_create1(0);// 0表示使用默认行为
    if (epfd < 0) {
        std::cerr << "Error: epoll_create1 failed" << std::endl;
        return;
    }

    struct epoll_event server_event;
    server_event.data.fd = server_fd_;
    server_event.events = EPOLLIN;

    int res = epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd_, &server_event);
    if (res < 0){
        std::cerr << "Error: epoll_ctl add server_fd failed" << std::endl;
        close(epfd);
        return;
    }

    const int MAX_EVENTS = 1024;  // 一次最多处理1024个就绪事件
    struct epoll_event ready_events[MAX_EVENTS];  // 仅用于接收就绪事件

    while (true) {
        // 等待活动（阻塞，无超时）
        int nfds = epoll_wait(epfd, ready_events, MAX_EVENTS, -1); // 不设置超时
        if (nfds < 0 && errno != EINTR) {  // 忽略信号中断
            std::cerr << "Error: epoll_wait failed" << std::endl;
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            // 处理新连接（服务器socket有活动）
            if (ready_events[i].data.fd == server_fd_) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                while(true){ // 循环处理所有等待的连接
                    int new_socket = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
                    if (new_socket < 0) {
                        // 非阻塞模式下，无更多连接时返回EAGAIN，退出循环
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            std::cerr << "Error: errno == EAGAIN || errno == EWOULDBLOCK" << std::endl;
                            break;
                        } else {
                            std::cerr << "Error: accept failed" << std::endl;
                            break;
                        }
                    }
                    // 打印新连接信息
                    std::cout << "New connection from "
                              << inet_ntoa(client_addr.sin_addr)
                              << ":" << ntohs(client_addr.sin_port)
                              << " (socket: " << new_socket << ")" << std::endl;

                    // 将新连接加入客户端列表
                    struct epoll_event client_epollfd;
                    client_epollfd.data.fd = new_socket;
                    client_epollfd.events = EPOLLIN;  // 关注读事件（客户端发数据）

                    // 将新客户端socket添加到epoll实例
                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, new_socket, &client_epollfd) < 0) {
                        std::cerr << "Error: epoll_ctl add client_fd failed" << std::endl;
                        close(new_socket);
                        continue;
                    }
                }
            }
            else if(ready_events[i].events & (EPOLLIN | EPOLLERR | EPOLLHUP)){
                auto it = ready_events[i];
                int client_socket = it.data.fd;

                // 若触发EPOLLERR（连接错误）和 EPOLLHUP（连接挂断）事件（无需主动注册），直接关闭连接（无需创建子进程）
                if (it.events & (EPOLLERR | EPOLLHUP)) {
                    std::cerr << "Client connection error/hung up: " << client_socket << std::endl;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, client_socket, nullptr);
                    close(client_socket);
                    continue;
                }

                // 创建子进程处理请求
                pid_t pid = fork();
                if (pid < 0) {
                    std::cerr << "Error: fork failed" << std::endl;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, client_socket, nullptr); // 移除无效连接
                    close(client_socket);
                }
                else if (pid == 0) {
                    // 子进程：处理客户端请求
                    close(server_fd_);  // 子进程不需要服务器socket
                    close(epfd);         // 关闭epoll句柄（子进程不需要）
                    handle_client(client_socket);
                    exit(0);  // 处理完毕退出
                }
                else {
                    // 父进程：关闭客户端socket，从列表移除
                    epoll_ctl(epfd, EPOLL_CTL_DEL, client_socket, nullptr);
                    close(client_socket);
                }
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

