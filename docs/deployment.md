# TBOX PROV Service 部署文档

## 概述

本文档描述TBOX PROV服务的部署流程，包括环境准备、依赖安装、配置说明、启动和监控等。

## 环境要求

### 硬件要求

- **处理器**：ARM Cortex-A53 或更高
- **内存**：≥ 256MB
- **存储**：≥ 1GB 可用空间
- **网络**：支持CAN总线或以太网

### 软件要求

- **操作系统**：Linux (Yocto/Debian/Ubuntu)
- **内核版本**：≥ 4.14
- **C++运行时**：libstdc++ 6
- **依赖库**：[iov-vehicle-tbox-framework](../iov-vehicle-tbox-framework)（提供 yaml-cpp、spdlog、配置管理）, nlohmann_json

## 依赖安装

### 使用Conan安装

```bash
# 安装Conan
pip install conan

# 配置Conan profile
conan profile detect --force

# 安装依赖
conan install . --output-folder=build --build=missing
```

### 手动安装依赖

#### nlohmann_json

```bash
# 下载头文件
wget https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp
sudo mkdir -p /usr/local/include/nlohmann
sudo mv json.hpp /usr/local/include/nlohmann/
```

#### iov-vehicle-tbox-framework

```bash
# 编译安装框架
cd ../iov-vehicle-tbox-framework
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
```

## 编译部署

### 本地编译

```bash
# 克隆代码
git clone <repository_url>
cd iov-vehicle-tbox-prov

# 安装依赖
conan install . --output-folder=build --build=missing

# 编译
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### 交叉编译

```bash
# 安装交叉编译工具链
sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# 配置交叉编译
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain-aarch64-linux-gnu.cmake \
         -DCMAKE_BUILD_TYPE=Release

# 编译
make -j$(nproc)
```

### Docker编译

```bash
# 构建Docker镜像
docker build -t tbox-prov-builder -f Dockerfile.cross .

# 运行编译
docker run -v $(pwd):/workspace tbox-prov-builder
```

## 配置说明

### 配置文件

使用框架 `ConfigManager` 三层配置体系：

| 层级 | 文件路径 | 是否必需 |
|------|----------|----------|
| Common | `/etc/tbox/common.yaml` | 是 |
| Service | `/etc/tbox/conf.d/prov.yaml` | 否 |
| Local | `./prov.yaml`（当前工作目录） | 否 |

服务专属配置示例（`/etc/tbox/conf.d/prov.yaml`）：

```yaml
# ECU配置
ecu:
  uid: "00000000000000000000000000000001"  # 仅测试环境

# 存储配置
storage:
  path: "/var/tbox/prov"
  enable_write_protection: true
  max_retry_count: 3

# UDS配置
uds:
  security_access:
    supported_levels:
      - 0x27
      - 0x29
    seed_length: 4
    key_length: 4

# DID配置
did:
  vin: 0xF190
  vehicle_binding: 0xF191
  provision_state: 0xF192
  vehicle_config: 0xF193
  production_info: 0xF194

# RID配置
rid:
  rewrite_routine: 0xFF00

# 日志配置
logging:
  level: "INFO"
  file: "/var/log/tbox/prov.log"
  max_size: 10485760
  max_files: 5
```

### 环境变量

| 变量名 | 说明 | 默认值 |
|--------|------|--------|
| PROV_STORAGE_PATH | 存储路径 | /var/tbox/prov |

## 启动服务

### 手动启动

```bash
# 启动服务
./TboxProvService

# 或者指定配置文件
./TboxProvService --config /path/to/config.yaml
```

### Systemd服务

创建服务文件 `/etc/systemd/system/tbox-prov.service`：

```ini
[Unit]
Description=TBOX PROV Service
After=network.target

[Service]
Type=simple
User=tbox
Group=tbox
ExecStart=/usr/bin/TboxProvService
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

启动服务：

```bash
# 重新加载systemd配置
sudo systemctl daemon-reload

# 启动服务
sudo systemctl start tbox-prov

# 设置开机自启
sudo systemctl enable tbox-prov

# 查看服务状态
sudo systemctl status tbox-prov
```

### 启动脚本

创建启动脚本 `/usr/bin/tbox-prov-start.sh`：

```bash
#!/bin/bash

# 创建必要目录
mkdir -p /var/tbox/prov
mkdir -p /var/log/tbox

# 启动服务
exec /usr/bin/TboxProvService "$@"
```

## 监控和日志

### 日志查看

```bash
# 查看实时日志
tail -f /var/log/tbox/prov.log

# 查看系统日志
journalctl -u tbox-prov -f

# 查看错误日志
grep -i error /var/log/tbox/prov.log
```

### 健康检查

```bash
# 检查服务状态
systemctl status tbox-prov

# 检查进程是否存在
ps aux | grep TboxProvService

# 检查端口监听（如果使用网络）
netstat -tlnp | grep tbox
```

### 性能监控

```bash
# 查看CPU和内存使用
top -p $(pgrep TboxProvService)

# 查看磁盘使用
du -sh /var/tbox/prov

# 查看I/O统计
iostat -x 1
```

## 故障排查

### 常见问题

1. **服务启动失败**
   - 检查配置文件是否存在
   - 检查存储目录权限
   - 检查依赖库是否安装

2. **UDS通信失败**
   - 检查CAN总线连接
   - 检查UDS服务配置
   - 检查安全访问配置

3. **存储错误**
   - 检查磁盘空间
   - 检查文件权限
   - 检查NVM分区状态

### 调试方法

1. **启用调试日志**
   ```yaml
   logging:
     level: "DEBUG"
   ```

2. **使用GDB调试**
   ```bash
   gdb ./TboxProvService
   (gdb) run
   (gdb) bt  # 查看调用栈
   ```

3. **检查核心转储**
   ```bash
   ulimit -c unlimited
   # 重现问题后
   gdb ./TboxProvService core
   ```

## 升级和维护

### 版本升级

1. **备份数据**
   ```bash
   cp -r /var/tbox/prov /var/tbox/prov.backup
   ```

2. **停止服务**
   ```bash
   sudo systemctl stop tbox-prov
   ```

3. **更新二进制文件**
   ```bash
   sudo cp TboxProvService /usr/bin/
   sudo chmod +x /usr/bin/TboxProvService
   ```

4. **更新配置（如有必要）**
   ```bash
   sudo cp prov.yaml /etc/tbox/conf.d/prov.yaml
   ```

5. **启动服务**
   ```bash
   sudo systemctl start tbox-prov
   ```

6. **验证服务**
   ```bash
   sudo systemctl status tbox-prov
   ```

### 数据维护

1. **清理日志**
   ```bash
   # 清理旧日志
   find /var/log/tbox -name "*.log" -mtime +30 -delete
   
   # 或者使用logrotate
   sudo logrotate -f /etc/logrotate.d/tbox-prov
   ```

2. **备份存储数据**
   ```bash
   # 备份绑定数据
   tar -czf prov-backup-$(date +%Y%m%d).tar.gz /var/tbox/prov
   ```

3. **重置服务**
   ```bash
   # 停止服务
   sudo systemctl stop tbox-prov
   
   # 清理数据
   rm -rf /var/tbox/prov/*
   
   # 启动服务
   sudo systemctl start tbox-prov
   ```

## 安全注意事项

1. **文件权限**
   - 配置文件：600 (root:root)
   - 存储目录：700 (tbox:tbox)
   - 日志文件：640 (tbox:tbox)

2. **网络访问**
   - 限制UDS服务访问权限
   - 启用安全访问机制
   - 记录所有访问日志

3. **数据保护**
   - 启用写保护机制
   - 定期备份数据
   - 加密敏感数据