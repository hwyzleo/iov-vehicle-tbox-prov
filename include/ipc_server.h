#pragma once

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <memory>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "ipc_protocol.h"

namespace tbox {
namespace prov {

class ProvService;  // 前向声明

namespace ipc {

class IpcServer {
public:
    using RequestHandler = std::function<std::string(const std::string&)>;
    
    IpcServer(const std::string& socket_path = DEFAULT_SOCKET_PATH);
    ~IpcServer();
    
    // 启动服务器
    bool start(ProvService* service);
    
    // 停止服务器
    void stop();
    
    // 检查是否正在运行
    bool is_running() const { return running_; }
    
private:
    std::string socket_path_;
    int server_fd_;
    std::atomic<bool> running_;
    std::thread accept_thread_;
    ProvService* service_;
    
    // 接受连接
    void accept_connections();
    
    // 处理客户端连接
    void handle_client(int client_fd);
    
    // 处理请求
    std::string handle_request(const std::string& request_data);
};

} // namespace ipc
} // namespace prov
} // namespace tbox