# 任务 1：创建 ProvLogAdapter 声明与实现

## 任务描述

创建 ProvLogAdapter 类，封装 framework-log 的初始化和 7 个模块 Logger 的获取。

## 文件清单

- 创建：`include/prov_log_adapter.h`
- 创建：`src/prov_log_adapter.cpp`
- 创建：`tests/test_log_adapter.cpp`
- 修改：`CMakeLists.txt`

## framework-log API 参考

framework-log 头文件位置：`/Users/hwyz_leo/Projects/open-iov/vehicle/tbox/iov-vehicle-tbox-framework/include/log.h`

```cpp
// log.h
namespace tbox::fw::log {
class Logger {
public:
    static InitResult init(const std::string& service, const LogConfig& config);
    static Logger get(const std::string& module);
    
    void info(std::string_view event, std::string_view message,
              std::initializer_list<Field> fields = {});
    void warn(std::string_view event, std::string_view message,
              std::initializer_list<Field> fields = {});
    void error(std::string_view event, std::string_view message,
               std::initializer_list<Field> fields = {});
};

struct InitResult {
    LogError error;
    std::string error_message;
};

enum class LogError : uint32_t {
    kOk = 0,
    kConfigInvalid = 201,
    kInitFailed = 202,
    // ...
};
}
```

## LogAdapter 模式参考（来自 iov-vehicle-tbox-tsp）

```cpp
// include/log_adapter.h
#pragma once
#include "log.h"
#include "log_types.h"
#include <string>

namespace tbox::tsp {
class LogAdapter {
public:
    static tbox::fw::log::InitResult init(
        const std::string& service,
        const tbox::fw::log::LogConfig& config
    );
    static tbox::fw::log::Logger route();
    static tbox::fw::log::Logger fota();
    // ...
private:
    static bool s_initialized;
};
}
```

## 具体要求

### include/prov_log_adapter.h

```cpp
#pragma once
#include "log.h"
#include "log_types.h"
#include <string>

namespace tbox::prov {

class ProvLogAdapter {
public:
    static tbox::fw::log::InitResult init(
        const std::string& service,
        const tbox::fw::log::LogConfig& config
    );

    static tbox::fw::log::Logger service();
    static tbox::fw::log::Logger vin();
    static tbox::fw::log::Logger binding();
    static tbox::fw::log::Logger rewrite();
    static tbox::fw::log::Logger vehicle_config();
    static tbox::fw::log::Logger production();
    static tbox::fw::log::Logger ipc();

private:
    static bool s_initialized;
};

} // namespace tbox::prov
```

### src/prov_log_adapter.cpp

实现 init() 和 7 个 Logger::get() 调用，模式与 TSP/SEC 一致。

### tests/test_log_adapter.cpp

使用 GTest 测试：
1. InitSucceeds - 初始化成功
2. GetServiceLogger - 获取 service logger 非空
3. GetVinLogger - 获取 vin logger 非空
4. GetBindingLogger - 获取 binding logger 非空
5. GetRewriteLogger - 获取 rewrite logger 非空
6. GetVehicleConfigLogger - 获取 vehicle_config logger 非空
7. GetProductionLogger - 获取 production logger 非空
8. GetIpcLogger - 获取 ipc logger 非空

### CMakeLists.txt 修改

在 LIB_SOURCES 中添加 `src/prov_log_adapter.cpp`。
在 TboxProvTests 中添加 `tests/test_log_adapter.cpp`。

## 测试命令

```bash
cd build && cmake .. && make TboxProvTests && ./TboxProvTests --gtest_filter="ProvLogAdapter*"
```

## 提交信息

```
feat(prov): add ProvLogAdapter with 7 module loggers
```
