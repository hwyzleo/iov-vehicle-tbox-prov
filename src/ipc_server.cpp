#include "ipc_server.h"
#include "prov_service.h"
#include "ipc_protocol.h"
#include "prov_log_adapter.h"
#include "prov_context.h"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

namespace tbox {
namespace prov {
namespace ipc {

IpcServer::IpcServer(const std::string& socket_path) 
    : socket_path_(socket_path), server_fd_(-1), running_(false), service_(nullptr) {
    shutdown_pipe_[0] = -1;
    shutdown_pipe_[1] = -1;
}

IpcServer::~IpcServer() {
    stop();
}

bool IpcServer::start(ProvService* service) {
    if (running_) {
        return true;
    }
    
    service_ = service;
    
    // 创建 shutdown pipe
    if (pipe(shutdown_pipe_) < 0) {
        ProvLogAdapter::ipc().error("prov.ipc.start_failed",
            "Failed to create shutdown pipe",
            {tbox::fw::log::Field("error", tbox::fw::log::FieldValue::makeString(strerror(errno)))}
        );
        return false;
    }
    
    // 创建 Unix Socket
    server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        ProvLogAdapter::ipc().error("prov.ipc.start_failed",
            "Failed to create socket",
            {tbox::fw::log::Field("error", tbox::fw::log::FieldValue::makeString(strerror(errno)))}
        );
        return false;
    }
    
    // 设置 socket 选项
    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        ProvLogAdapter::ipc().error("prov.ipc.start_failed",
            "Failed to set socket options",
            {tbox::fw::log::Field("error", tbox::fw::log::FieldValue::makeString(strerror(errno)))}
        );
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
        ProvLogAdapter::ipc().error("prov.ipc.start_failed",
            "Failed to bind socket",
            {tbox::fw::log::Field("socket_path", tbox::fw::log::FieldValue::makeString(socket_path_)),
             tbox::fw::log::Field("error", tbox::fw::log::FieldValue::makeString(strerror(errno)))}
        );
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }
    
    // 监听连接
    if (listen(server_fd_, 5) < 0) {
        ProvLogAdapter::ipc().error("prov.ipc.start_failed",
            "Failed to listen on socket",
            {tbox::fw::log::Field("error", tbox::fw::log::FieldValue::makeString(strerror(errno)))}
        );
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }
    
    running_ = true;
    accept_thread_ = std::thread(&IpcServer::accept_connections, this);
    
    ProvLogAdapter::ipc().info("prov.ipc.started",
        "IPC server started",
        {tbox::fw::log::Field("socket_path", tbox::fw::log::FieldValue::makeString(socket_path_))}
    );
    return true;
}

void IpcServer::stop() {
    if (!running_) {
        return;
    }
    
    ProvLogAdapter::ipc().info("prov.ipc.stopping",
        "IPC server stopping"
    );
    running_ = false;
    
    // 通过 shutdown pipe 唤醒阻塞在 select/accept 上的线程
    if (shutdown_pipe_[1] >= 0) {
        char c = 1;
        write(shutdown_pipe_[1], &c, 1);
    }
    
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
    
    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
    }
    
    if (shutdown_pipe_[0] >= 0) { close(shutdown_pipe_[0]); shutdown_pipe_[0] = -1; }
    if (shutdown_pipe_[1] >= 0) { close(shutdown_pipe_[1]); shutdown_pipe_[1] = -1; }
    
    // 删除 socket 文件
    unlink(socket_path_.c_str());
    
    ProvLogAdapter::ipc().info("prov.ipc.stopped",
        "IPC server stopped"
    );
}

void IpcServer::accept_connections() {
    ProvLogAdapter::ipc().debug("prov.ipc.accept_thread_started",
        "Accept thread started",
        {tbox::fw::log::Field("server_fd", tbox::fw::log::FieldValue::makeInt(server_fd_))}
    );
    
    while (running_) {
        // 用 select 同时监听 server_fd 和 shutdown_pipe
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(server_fd_, &rfds);
        FD_SET(shutdown_pipe_[0], &rfds);
        int maxfd = (server_fd_ > shutdown_pipe_[0]) ? server_fd_ : shutdown_pipe_[0];
        
        int ret = select(maxfd + 1, &rfds, nullptr, nullptr, nullptr);
        if (ret < 0) {
            if (errno == EINTR) continue;
            ProvLogAdapter::ipc().error("prov.ipc.accept_failed",
                "select failed",
                {tbox::fw::log::Field("error", tbox::fw::log::FieldValue::makeString(strerror(errno)))}
            );
            break;
        }
        
        // shutdown pipe 被唤醒，退出循环
        if (FD_ISSET(shutdown_pipe_[0], &rfds)) {
            ProvLogAdapter::ipc().debug("prov.ipc.shutdown_signaled",
                "Shutdown pipe signaled, exiting"
            );
            break;
        }
        
        if (!FD_ISSET(server_fd_, &rfds)) {
            continue;
        }
        
        int client_fd = accept(server_fd_, nullptr, nullptr);
        if (client_fd < 0) {
            if (running_) {
                ProvLogAdapter::ipc().error("prov.ipc.accept_failed",
                    "accept failed",
                    {tbox::fw::log::Field("error", tbox::fw::log::FieldValue::makeString(strerror(errno)))}
                );
            }
            continue;
        }
        
        ProvLogAdapter::ipc().info("prov.ipc.client_connected",
            "New client connection",
            {tbox::fw::log::Field("client_fd", tbox::fw::log::FieldValue::makeInt(client_fd))}
        );
        
        // 在新线程中处理客户端连接
        std::thread client_thread(&IpcServer::handle_client, this, client_fd);
        client_thread.detach();
    }
    
    ProvLogAdapter::ipc().debug("prov.ipc.accept_thread_exiting",
        "Accept thread exiting"
    );
}

void IpcServer::handle_client(int client_fd) {
    ProvLogAdapter::ipc().debug("prov.ipc.client_handler_started",
        "Client handler started",
        {tbox::fw::log::Field("client_fd", tbox::fw::log::FieldValue::makeInt(client_fd))}
    );
    
    try {
        // 设置空闲超时（60秒无数据则断开）
        struct timeval tv;
        tv.tv_sec = 60;
        tv.tv_usec = 0;
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
        
        // 长连接循环处理多个请求
        while (true) {
            // 读取请求头
            RequestHeader header;
            memset(&header, 0, sizeof(header));
            size_t header_received = 0;
            while (header_received < sizeof(header)) {
                ssize_t bytes_read = recv(client_fd, reinterpret_cast<uint8_t*>(&header) + header_received, sizeof(header) - header_received, 0);
                if (bytes_read <= 0) {
                    if (bytes_read == 0) {
                        ProvLogAdapter::ipc().info("prov.ipc.client_disconnected",
                            "Connection closed by peer",
                            {tbox::fw::log::Field("client_fd", tbox::fw::log::FieldValue::makeInt(client_fd))}
                        );
                    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        ProvLogAdapter::ipc().info("prov.ipc.client_timeout",
                            "Idle timeout, closing",
                            {tbox::fw::log::Field("client_fd", tbox::fw::log::FieldValue::makeInt(client_fd))}
                        );
                    } else {
                        ProvLogAdapter::ipc().error("prov.ipc.recv_header_failed",
                            "recv header failed",
                            {tbox::fw::log::Field("client_fd", tbox::fw::log::FieldValue::makeInt(client_fd)),
                             tbox::fw::log::Field("error", tbox::fw::log::FieldValue::makeString(strerror(errno)))}
                        );
                    }
                    close(client_fd);
                    return;
                }
                header_received += bytes_read;
            }
            
            // 设置上下文（生成请求ID用于日志关联）
            std::string request_id = generate_request_id();
            
            auto scope = make_context_scope(request_id, request_id, "");
            
            ProvLogAdapter::ipc().info("prov.ipc.request_received",
                "IPC request received",
                {tbox::fw::log::Field("client_fd", tbox::fw::log::FieldValue::makeInt(client_fd)),
                 tbox::fw::log::Field("method_id", tbox::fw::log::FieldValue::makeInt(static_cast<int>(header.method_id))),
                 tbox::fw::log::Field("params_length", tbox::fw::log::FieldValue::makeInt(header.params_length)),
                 tbox::fw::log::Field("request_id", tbox::fw::log::FieldValue::makeString(request_id))}
            );
            
            // 合理性检查
            if (header.params_length > 10 * 1024 * 1024) {  // 最大 10MB
                ProvLogAdapter::ipc().error("prov.ipc.request_too_large",
                    "Request too large",
                    {tbox::fw::log::Field("client_fd", tbox::fw::log::FieldValue::makeInt(client_fd)),
                     tbox::fw::log::Field("params_length", tbox::fw::log::FieldValue::makeInt(header.params_length))}
                );
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
                    ProvLogAdapter::ipc().error("prov.ipc.recv_params_failed",
                        "recv params failed",
                        {tbox::fw::log::Field("client_fd", tbox::fw::log::FieldValue::makeInt(client_fd))}
                    );
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
                    ProvLogAdapter::ipc().error("prov.ipc.send_response_failed",
                        "send response failed",
                        {tbox::fw::log::Field("client_fd", tbox::fw::log::FieldValue::makeInt(client_fd)),
                         tbox::fw::log::Field("error", tbox::fw::log::FieldValue::makeString(strerror(errno)))}
                    );
                    close(client_fd);
                    return;
                }
                total_sent += bytes_sent;
            }
            
            ProvLogAdapter::ipc().debug("prov.ipc.response_sent",
                "Response sent, waiting for next request",
                {tbox::fw::log::Field("client_fd", tbox::fw::log::FieldValue::makeInt(client_fd))}
            );
        }
    } catch (const std::length_error& e) {
        ProvLogAdapter::ipc().error("prov.ipc.exception",
            "std::length_error",
            {tbox::fw::log::Field("client_fd", tbox::fw::log::FieldValue::makeInt(client_fd)),
             tbox::fw::log::Field("what", tbox::fw::log::FieldValue::makeString(e.what()))}
        );
    } catch (const std::bad_alloc& e) {
        ProvLogAdapter::ipc().error("prov.ipc.exception",
            "std::bad_alloc",
            {tbox::fw::log::Field("client_fd", tbox::fw::log::FieldValue::makeInt(client_fd)),
             tbox::fw::log::Field("what", tbox::fw::log::FieldValue::makeString(e.what()))}
        );
    } catch (const std::exception& e) {
        ProvLogAdapter::ipc().error("prov.ipc.exception",
            "std::exception",
            {tbox::fw::log::Field("client_fd", tbox::fw::log::FieldValue::makeInt(client_fd)),
             tbox::fw::log::Field("what", tbox::fw::log::FieldValue::makeString(e.what()))}
        );
    } catch (...) {
        ProvLogAdapter::ipc().error("prov.ipc.exception",
            "unknown exception type",
            {tbox::fw::log::Field("client_fd", tbox::fw::log::FieldValue::makeInt(client_fd))}
        );
    }
    
    close(client_fd);
}

std::string IpcServer::handle_request(const std::string& request_data) {
    // 解析请求
    MethodId method;
    std::string params_json;
    
    try {
        if (!IpcSerializer::deserialize_request(std::vector<uint8_t>(request_data.begin(), request_data.end()), method, params_json)) {
            return std::string(IpcSerializer::serialize_response(-1, "{\"error\":\"Invalid request\"}").begin(),
                              IpcSerializer::serialize_response(-1, "{\"error\":\"Invalid request\"}").end());
        }
    } catch (const std::exception& e) {
        return std::string(IpcSerializer::serialize_response(-1, "{\"error\":\"Deserialize failed\"}").begin(),
                          IpcSerializer::serialize_response(-1, "{\"error\":\"Deserialize failed\"}").end());
    }
    
    // 调用相应的服务方法
    std::string result_json;
    int32_t status_code = 0;
    
    try {
        ProvLogAdapter::ipc().debug("prov.ipc.dispatch",
            "Dispatching request",
            {tbox::fw::log::Field("method", tbox::fw::log::FieldValue::makeInt(static_cast<int>(method)))}
        );
        
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
                result_json = "{\"success\":false,\"error\":\"Not implemented\"}";
                status_code = -1;
                break;
            }
            case MethodId::WRITE_PRODUCTION_INFO: {
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
        ProvLogAdapter::ipc().error("prov.ipc.dispatch_exception",
            "Exception in dispatch",
            {tbox::fw::log::Field("what", tbox::fw::log::FieldValue::makeString(e.what()))}
        );
        result_json = "{\"error\":\"" + std::string(e.what()) + "\"}";
        status_code = -1;
    } catch (...) {
        ProvLogAdapter::ipc().error("prov.ipc.dispatch_exception",
            "Unknown exception in dispatch"
        );
        result_json = "{\"error\":\"Unknown exception in dispatch\"}";
        status_code = -1;
    }
    
    // 确保 result_json 不为空
    if (result_json.empty()) {
        result_json = "{\"error\":\"Unknown error\"}";
        status_code = -1;
    }
    
    try {
        auto resp = IpcSerializer::serialize_response(status_code, result_json);
        return std::string(resp.begin(), resp.end());
    } catch (const std::exception& e) {
        ProvLogAdapter::ipc().error("prov.ipc.serialize_failed",
            "serialize_response exception",
            {tbox::fw::log::Field("what", tbox::fw::log::FieldValue::makeString(e.what()))}
        );
        throw;
    }
}

} // namespace ipc
} // namespace prov
} // namespace tbox
