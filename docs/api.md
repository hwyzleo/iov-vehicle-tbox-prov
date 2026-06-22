# PROV 服务 API 文档

## 概述

PROV 服务提供车辆个性化功能，包括 VIN 写入、车辆绑定、配置写入和生产信息写入。

## 接口说明

### 初始化服务

```cpp
ErrorCode initialize();
```

初始化 PROV 服务，包括初始化存储系统。

**返回值:**
- `ErrorCode::SUCCESS` - 初始化成功
- 其他错误码 - 初始化失败

### 写入 VIN

```cpp
ErrorCode write_vin(const std::string& vin);
```

写入 VIN 码并建立车辆-TBOX 绑定。

**参数:**
- `vin` - 17 位 VIN 码

**返回值:**
- `ErrorCode::SUCCESS` - 写入成功
- `ErrorCode::INVALID_STATE` - 服务未初始化
- `ErrorCode::INVALID_VIN_FORMAT` - VIN 格式非法
- `ErrorCode::VIN_WRITE_FAILED` - VIN 写入失败
- `ErrorCode::READBACK_VERIFICATION_FAILED` - 回读校验失败
- `ErrorCode::ECU_UID_READ_ERROR` - ECU UID 读取失败

### 读取 VIN

```cpp
std::string read_vin() const;
```

读取已写入的 VIN 码。

**返回值:**
- VIN 码字符串，如果未写入则返回空字符串

### 读取绑定信息

```cpp
VehicleBinding read_binding() const;
```

读取车辆-TBOX 绑定信息。

**返回值:**
- `VehicleBinding` 结构体，包含 VIN、ECU UID、绑定状态等信息

### 获取个性化状态

```cpp
ProvisionState get_provision_state() const;
```

获取当前个性化状态。

**返回值:**
- `ProvisionState::NONE` - 未开始
- `ProvisionState::VIN_WRITTEN` - VIN 已写入待绑定
- `ProvisionState::BOUND` - 已绑定
- `ProvisionState::FAILED` - 失败

### 写入车辆配置

```cpp
ErrorCode write_vehicle_config(const std::vector<uint8_t>& config_data);
```

写入车辆配置（变量编码）。

**参数:**
- `config_data` - 配置数据字节数组

**返回值:**
- `ErrorCode::SUCCESS` - 写入成功
- `ErrorCode::INVALID_STATE` - 服务未初始化
- `ErrorCode::SECURITY_ACCESS_NOT_GRANTED` - 写保护已启用
- `ErrorCode::CONFIG_WRITE_FAILED` - 配置写入失败
- `ErrorCode::READBACK_VERIFICATION_FAILED` - 回读校验失败

### 写入生产信息

```cpp
ErrorCode write_production_info(const ProductionInfo& info);
```

写入生产溯源信息。

**参数:**
- `info` - 生产信息结构体

**返回值:**
- `ErrorCode::SUCCESS` - 写入成功
- `ErrorCode::INVALID_STATE` - 服务未初始化
- `ErrorCode::SECURITY_ACCESS_NOT_GRANTED` - 写保护已启用
- `ErrorCode::PRODUCTION_INFO_WRITE_FAILED` - 生产信息写入失败
- `ErrorCode::READBACK_VERIFICATION_FAILED` - 回读校验失败

### 授权重写

```cpp
ErrorCode authorize_rewrite(const std::string& new_vin);
```

授权重写 VIN（用于售后/换件场景）。

**参数:**
- `new_vin` - 新的 VIN 码

**返回值:**
- `ErrorCode::SUCCESS` - 重写成功
- `ErrorCode::INVALID_STATE` - 服务未初始化
- `ErrorCode::INVALID_VIN_FORMAT` - VIN 格式非法
- `ErrorCode::VIN_CONFLICT_UNAUTHORIZED` - VIN 冲突未授权
- `ErrorCode::VIN_WRITE_FAILED` - VIN 写入失败
- `ErrorCode::READBACK_VERIFICATION_FAILED` - 回读校验失败

## 错误码

| 错误码 | 含义 | 处理建议 |
|--------|------|----------|
| PROV-1001 | 服务未初始化 | 检查服务初始化状态 |
| PROV-1002 | 安全访问未解锁 / 写保护已启用 | 检查写保护状态 |
| PROV-1003 | VIN 格式非法 | 验证 VIN 码格式 |
| PROV-1004 | VIN 写入失败 | 检查存储状态，重试 |
| PROV-1005 | 回读校验不一致 | 检查存储可靠性，重试 |
| PROV-1006 | VIN 冲突未授权 | 需要授权重写 |
| PROV-1007 | 配置写入失败 | 检查存储状态，重试 |
| PROV-1008 | 生产信息写入失败 | 检查存储状态，重试 |
| PROV-1009 | ECU UID 读取失败 | 检查硬件状态 |

## 与 DIAG 服务集成

PROV 服务通过 DIAG 服务的 `RealProvAdapter` 适配器进行调用。DIAG 服务负责：
- 安全访问检查（0x27/0x29）
- UDS 协议处理
- DID/RID 路由
- DTC/NRC 映射

PROV 服务专注于业务逻辑，不感知诊断细节。
