# TBOX PROV Service

TBOX 产线个性化（PROV）服务，负责在总装阶段通过 UDS 协议完成 VIN 写入、ECU UID 读取、车辆绑定、配置和生产信息写入等功能。

## 功能特性

- **VIN 写入与绑定**：通过 UDS 协议写入 VIN 码，建立 VIN↔ECU UID 绑定
- **授权重写**：支持在安全访问授权下的 VIN 重写
- **配置写入**：写入车辆配置和变量编码
- **生产信息写入**：写入生产日期、批次号等信息
- **写保护机制**：默认锁定，防止未授权写入
- **回读校验**：写入后回读验证数据一致性

## 项目结构

```
iov-vehicle-tbox-prov/
├── CMakeLists.txt              # CMake 构建配置
├── conanfile.txt               # Conan 依赖配置
├── README.md                   # 项目说明
├── AGENTS.md                   # 开发约束文档
├── include/                    # 头文件
│   ├── prov_service.h          # PROV 服务主类
│   ├── uds_handler.h           # UDS 处理器
│   ├── protected_storage.h     # 受保护存储接口
│   ├── protected_storage_impl.h # 受保护存储实现
│   ├── ecu_uid.h               # ECU UID 读取
│   ├── vin_validator.h         # VIN 格式校验
│   ├── data_models.h           # 数据模型定义
│   ├── error_codes.h           # 错误码定义
│   └── constants.h             # 常量定义
├── src/                        # 源文件
│   ├── main.cpp                # 主程序入口
│   ├── prov_service.cpp        # PROV 服务实现
│   ├── uds_handler.cpp         # UDS 处理器实现
│   ├── protected_storage.cpp   # 受保护存储实现
│   ├── ecu_uid.cpp             # ECU UID 读取实现
│   ├── vin_validator.cpp       # VIN 格式校验实现
│   ├── data_models.cpp         # 数据模型实现
│   └── error_codes.cpp         # 错误码实现
├── tests/                      # 测试文件
│   ├── test_prov_service.cpp   # PROV 服务测试
│   ├── test_uds_handler.cpp    # UDS 处理器测试
│   ├── test_protected_storage.cpp # 受保护存储测试
│   ├── test_vin_validator.cpp  # VIN 格式校验测试
│   └── test_data_models.cpp    # 数据模型测试
├── docs/                       # 文档
│   ├── api.md                  # API 文档
│   ├── diagnostic.md           # 诊断规范
│   ├── deployment.md           # 部署文档
│   └── build-and-verify.md     # 编译与验证指南
├── config/                     # 配置文件
│   └── prov_config.yaml        # 服务配置文件示例（部署为 /etc/tbox/conf.d/prov.yaml）
├── scripts/                    # 脚本
│   ├── build.sh                # 构建脚本
│   └── test.sh                 # 测试脚本
└── toolchain-aarch64-linux-gnu.cmake # 交叉编译工具链
```

## 快速开始

> **详细编译指南**：本地编译 vs Docker 交叉编译的区别和使用场景，请参考 [编译与验证指南](docs/build-and-verify.md)

### 环境要求

- C++17 编译器
- CMake ≥ 3.10
- Conan ≥ 2.0
- [iov-vehicle-tbox-framework](../iov-vehicle-tbox-framework) - HWYZ 框架（提供配置管理、yaml-cpp、spdlog）
- nlohmann_json
- Google Test（用于测试）

### 编译安装

#### 本地编译

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

#### 交叉编译

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

### 运行测试

```bash
# 运行所有测试
cd build
ctest --output-on-failure

# 或者使用测试脚本
./scripts/test.sh all
```

### 启动服务

```bash
# 手动启动
./TboxProvService
```

## 配置说明

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

# DID配置
did:
  vin: 0xF190
  vehicle_binding: 0xF191
  provision_state: 0xF192
  vehicle_config: 0xF193
  production_info: 0xF194
```

## API 使用

### 初始化服务

```cpp
#include "config.h"
#include "prov_service.h"

// 加载框架配置
auto err = CONFIG_MANAGER.load("prov");
if (err != hwyz::config::ConfigError::kOk) {
    return 1;
}

// 从配置读取服务参数
auto cfg = CONFIG_SNAPSHOT;
tbox::prov::ProvServiceConfig config;
config.storage_path = cfg->getString("storage.path", "/var/tbox/prov");
config.enable_write_protection = cfg->getBool("storage.enable_write_protection", true);
config.max_retry_count = cfg->getInt("storage.max_retry_count", 3);

tbox::prov::ProvService service(config);
auto result = service.initialize();
```

### 写入 VIN

```cpp
auto result = service.write_vin("1HGBH41JXMN109186");
if (result == tbox::prov::ErrorCode::SUCCESS) {
    // 写入成功
}
```

### 读取 VIN

```cpp
std::string vin = service.read_vin();
```

### 获取状态

```cpp
auto state = service.get_provision_state();
```

## 错误码

| 错误码 | 含义 | 处理建议 |
|--------|------|----------|
| PROV-1001 | 安全访问未解锁 | 检查 UDS 安全访问流程 |
| PROV-1002 | VIN 格式非法 | 验证 VIN 码格式 |
| PROV-1003 | VIN 写入失败 | 检查存储状态，重试 |
| PROV-1004 | 回读校验不一致 | 检查存储可靠性，重试 |
| PROV-1005 | VIN 冲突未授权 | 需要授权重写 |
| PROV-1006 | 配置写入失败 | 检查存储状态，重试 |
| PROV-1007 | 生产信息写入失败 | 检查存储状态，重试 |

## 文档

- [API 文档](docs/api.md) - 详细的 API 说明
- [诊断规范](docs/diagnostic.md) - UDS 诊断接口规范
- [部署文档](docs/deployment.md) - 部署和运维指南
- [编译与验证指南](docs/build-and-verify.md) - 本地编译 vs Docker 交叉编译

## 依赖项目

- [iov-vehicle-tbox-sec](../iov-vehicle-tbox-sec) - 安全服务（提供安全访问机制）
- [iov-vehicle-tbox-framework](../iov-vehicle-tbox-framework) - 基础框架