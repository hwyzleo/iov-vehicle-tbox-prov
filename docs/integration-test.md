# PROV 与 DIAG 集成测试指南

## 概述

本文档说明如何测试 PROV 服务与 DIAG 服务的集成。

## 前提条件

1. DIAG 服务已正确配置
2. PROV 服务已实现并编译
3. RealProvAdapter 已在 DIAG 服务中实现

## 测试步骤

### 1. 启动 DIAG 服务

```bash
cd iov-vehicle-tbox-diag/build
./DiagService
```

### 2. 启动 PROV 服务

```bash
cd iov-vehicle-tbox-prov/build
./TboxProvService
```

### 3. 发送 UDS 请求

使用诊断工具发送 UDS 请求：
- 安全访问（0x27）
- 写入 VIN（0x22 F190）
- 读取 VIN（0x22 F190）

### 4. 验证结果

检查 PROV 服务日志，确认：
- VIN 写入成功
- 绑定状态正确
- 错误码正确映射

## 预期结果

1. DIAG 服务正确处理安全访问
2. DIAG 服务通过 RealProvAdapter 调用 PROV 服务
3. PROV 服务正确执行业务逻辑
4. 错误码正确映射到 NRC

## 故障排除

### 问题：PROV 服务不可用

检查：
1. PROV 服务是否已启动
2. RealProvAdapter 是否正确初始化
3. 服务间通信是否正常

### 问题：安全访问失败

检查：
1. DIAG 服务的安全访问配置
2. SEC 服务是否正常工作
3. 安全访问密钥是否正确
