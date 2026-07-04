#include "ipc_server.h"
#include "prov_service.h"
#include "ipc_protocol.h"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

namespace tbox {
namespace prov {
namespace ipc {

IpcServer::IpcServer(const std::string& socket_path) 
    : socket_path_(socket_path), server_fd_(-1), running_(false), service_(nullptr) {
}

IpcServer::~IpcServer() {
    stop();
}

bool IpcServer::start(ProvService* service) {
    if (running_) {
        return true;
    }
    
    service_ = service;
    
    // 创建 Unix Socket
    server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
        return false;
    }
    
    // 设置 socket 选项
    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Failed to set socket options: " << strerror(errno) << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }
    
    // 绑定地址
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);
    
    // 删除已存在的 socket 文件
    unlink(socket_path_.c_str());
    
    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Failed to bind socket: " << strerror(errno) << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }
    
    // 监听连接
    if (listen(server_fd_, 5) < 0) {
        std::cerr << "Failed to listen on socket: " << strerror(errno) << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }
    
    running_ = true;
    accept_thread_ = std::thread(&IpcServer::accept_connections, this);
    
    std::cout << "IPC server started on " << socket_path_ << std::endl;
    return true;
}

void IpcServer::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
    }
    
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
    
    // 删除 socket 文件
    unlink(socket_path_.c_str());
    
    std::cout << "IPC server stopped" << std::endl;
}

void IpcServer::accept_connections() {
    while (running_) {
        int client_fd = accept(server_fd_, nullptr, nullptr);
        if (client_fd < 0) {
            if (running_) {
                std::cerr << "Failed to accept connection: " << strerror(errno) << std::endl;
            }
            continue;
        }
        
        // 在新线程中处理客户端连接
        std::thread client_thread(&IpcServer::handle_client, this, client_fd);
        client_thread.detach();
    }
}

void IpcServer::handle_client(int client_fd) {
    // 设置超时
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    
    // 每个连接只处理一个请求
    // 读取请求头
    RequestHeader header;
    memset(&header, 0, sizeof(header));
    size_t header_received = 0;
    while (header_received < sizeof(header)) {
        ssize_t bytes_read = recv(client_fd, reinterpret_cast<uint8_t*>(&header) + header_received, sizeof(header) - header_received, 0);
        if (bytes_read <= 0) {
            close(client_fd);
            return;
        }
        header_received += bytes_read;
    }
    
    // 合理性检查
    if (header.params_length > 10 * 1024 * 1024) {  // 最大 10MB
        std::cerr << "Request too large: " << header.params_length << std::endl;
        close(client_fd);
        return;
    }
    
    // 读取请求数据
    std::vector<uint8_t> request_data(sizeof(header) + header.params_length);
    memcpy(request_data.data(), &header, sizeof(header));
    
    size_t data_received = 0;
    while (data_received < header.params_length) {
        ssize_t bytes_read = recv(client_fd, request_data.data() + sizeof(header) + data_received, header.params_length - data_received, 0);
        if (bytes_read <= 0) {
            std::cerr << "Failed to read request data" << std::endl;
            close(client_fd);
            return;
        }
        data_received += bytes_read;
    }
    
    // 处理请求
    std::string response = handle_request(std::string(request_data.begin(), request_data.end()));
    
    // 发送响应
    size_t total_sent = 0;
    while (total_sent < response.size()) {
        ssize_t bytes_sent = send(client_fd, response.data() + total_sent, response.size() - total_sent, 0);
        if (bytes_sent <= 0) {
            std::cerr << "Failed to send response" << std::endl;
            break;
        }
        total_sent += bytes_sent;
    }
    
    close(client_fd);
}

std::string IpcServer::handle_request(const std::string& request_data) {
    // 解析请求
    MethodId method;
    std::string params_json;
    
    if (!IpcSerializer::deserialize_request(std::vector<uint8_t>(request_data.begin(), request_data.end()), method, params_json)) {
        return std::string(IpcSerializer::serialize_response(-1, "{\"error\":\"Invalid request\"}").begin(),
                          IpcSerializer::serialize_response(-1, "{\"error\":\"Invalid request\"}").end());
    }
    
    // 调用相应的服务方法
    std::string result_json;
    int32_t status_code = 0;
    
    try {
        switch (method) {
            case MethodId::INITIALIZE: {
                auto result = service_->initialize();
                result_json = "{\"success\":" + std::string(result == ErrorCode::SUCCESS ? "true" : "false") + "}";
                status_code = static_cast<int32_t>(result);
                break;
            }
            case MethodId::READ_VIN: {
                std::string vin = service_->read_vin();
                result_json = "{\"vin\":\"" + vin + "\"}";
                break;
            }
            case MethodId::READ_BINDING: {
                auto binding = service_->read_binding();
                result_json = "{\"vin\":\"" + binding.vin + "\","
                             "\"ecu_uid\":\"" + binding.ecu_uid + "\","
                             "\"state\":" + std::to_string(static_cast<int>(binding.state)) + ","
                             "\"locked\":" + (binding.locked ? "true" : "false") + "}";
                break;
            }
            case MethodId::GET_PROVISION_STATE: {
                auto state = service_->get_provision_state();
                result_json = "{\"state\":" + std::to_string(static_cast<int>(state)) + "}";
                break;
            }
            case MethodId::WRITE_VIN: {
                // 从 JSON 参数中提取 VIN
                // 简单实现：假设参数格式为 {"vin":"..."}
                // 实际应用中应使用 JSON 解析库
                size_t pos = params_json.find("\"vin\":\"");
                if (pos != std::string::npos) {
                    pos += 7;
                    size_t end = params_json.find("\"", pos);
                    if (end != std::string::npos) {
                        std::string vin = params_json.substr(pos, end - pos);
                        auto result = service_->write_vin(vin);
                        result_json = "{\"success\":" + std::string(result == ErrorCode::SUCCESS ? "true" : "false") + "}";
                        status_code = static_cast<int32_t>(result);
                    }
                }
                break;
            }
            case MethodId::WRITE_VEHICLE_CONFIG: {
                // 简化实现：实际应解析 JSON 数组
                result_json = "{\"success\":false,\"error\":\"Not implemented\"}";
                status_code = -1;
                break;
            }
            case MethodId::WRITE_PRODUCTION_INFO: {
                // 简化实现：实际应解析 JSON 对象
                result_json = "{\"success\":false,\"error\":\"Not implemented\"}";
                status_code = -1;
                break;
            }
            case MethodId::AUTHORIZE_REWRITE: {
                size_t pos = params_json.find("\"vin\":\"");
                if (pos != std::string::npos) {
                    pos += 7;
                    size_t end = params_json.find("\"", pos);
                    if (end != std::string::npos) {
                        std::string new_vin = params_json.substr(pos, end - pos);
                        auto result = service_->authorize_rewrite(new_vin);
                        result_json = "{\"success\":" + std::string(result == ErrorCode::SUCCESS ? "true" : "false") + "}";
                        status_code = static_cast<int32_t>(result);
                    }
                }
                break;
            }
            default:
                result_json = "{\"error\":\"Unknown method\"}";
                status_code = -1;
                break;
        }
    } catch (const std::exception& e) {
        result_json = "{\"error\":\"" + std::string(e.what()) + "\"}";
        status_code = -1;
    }
    
    // 确保 result_json 不为空
    if (result_json.empty()) {
        result_json = "{\"error\":\"Unknown error\"}";
        status_code = -1;
    }
    
    return std::string(IpcSerializer::serialize_response(status_code, result_json).begin(),
                      IpcSerializer::serialize_response(status_code, result_json).end());
}

} // namespace ipc
} // namespace prov
} // namespace tbox