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
| PROV-1001 | 安全访问未解锁 / 鉴权失败 | 拒绝写入 |
| PROV-1002 | VIN 格式非法 | 不写入，保持原状态 |
| PROV-1003 | VIN 写入失败 | 记录失败，可重试，不阻断下线 |
| PROV-1004 | 回读校验不一致 | 标记失败，不锁定，允许重写 |
| PROV-1005 | 已绑定且 VIN 冲突（未授权改写） | 拒绝，保留原绑定 |
| PROV-1006 | 配置/变量编码写入失败 | 可重试，不影响 VIN 绑定 |
| PROV-1007 | 生产信息写入失败 | 可重试，不影响 VIN 绑定与配置 |
| **PROV-1008** | **SE UID 读取失败/超时（SE 在位）** | **报错重试，不回退配置文件** |
| **PROV-1009** | **无 SE（硬件缺失）且配置文件缺失/无对应 UID** | **测试环境：报错，允许修正配置后重试** |
| **PROV-1010** | **生产环境无 SE，禁止配置文件兜底** | **fail-closed，拒绝建绑** |

## 与 DIAG 服务集成

PROV 服务通过 DIAG 服务的 `RealProvAdapter` 适配器进行调用。DIAG 服务负责：
- 安全访问检查（0x27/0x29）
- UDS 协议处理
- DID/RID 路由
- DTC/NRC 映射

PROV 服务专注于业务逻辑，不感知诊断细节。

## 获取 ECU UID

ECU UID 通过 `read_binding()` 接口获取：

```cpp
VehicleBinding binding = prov_service.read_binding();
std::string ecu_uid = binding.ecu_uid;
```

**使用场景：**
- SEC 服务构造 CSR 时需要 ECU UID
- RSMS 国标上报
- TSP 上线注册

## 测试环境配置

在测试环境（无 SE 硬件）中，PROV 服务支持从 YAML 配置文件读取 UID 作为兜底方案。

### 配置文件位置

使用框架 `ConfigManager` 三层配置体系：

| 层级 | 文件路径 | 是否必需 |
|------|----------|----------|
| Common | `/etc/tbox/common.yaml` | 是 |
| Service | `/etc/tbox/conf.d/prov.yaml` | 否 |
| Local | `./prov.yaml`（当前工作目录） | 否 |

### 配置文件格式

```yaml
# ECU配置
ecu:
  # ECU UID（仅测试环境使用，生产环境从SE读取）
  uid: "0123456789ABCDEF"
```

### 使用步骤

1. 在 `/etc/tbox/conf.d/prov.yaml` 或本地 `./prov.yaml` 中配置 `ecu.uid`

2. 重新编译（需要定义 `TEST_ENVIRONMENT` 编译宏）

### 注意事项

- 此配置**仅用于测试环境**
- 生产环境必须使用 SE 硬件读取 UID
- 生产环境无 SE 时会返回 `PROV-1010` 错误（fail-closed）
