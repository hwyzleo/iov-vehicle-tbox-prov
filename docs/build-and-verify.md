# TBOX PROV 编译与验证指南

## 编译方式对比

| 对比项 | 本地编译 | Docker 交叉编译 |
|--------|----------|-----------------|
| **编译环境** | 当前主机 (macOS/Linux) | Docker 容器 (ARM64 Linux) |
| **生成产物** | 主机可执行文件 | ARM64 Linux 可执行文件 |
| **用途** | 本地开发、调试、测试 | 部署到实际 TBOX 硬件 |
| **依赖安装** | 安装到本地系统 | 安装在容器内 |
| **是否污染本地** | 会 | 不会 |
| **编译速度** | 快 | 较慢（需构建镜像） |
| **环境一致性** | 依赖本地环境配置 | 完全一致 |

## 方式一：本地编译（开发调试用）

### 适用场景
- 日常开发和调试
- 快速验证代码修改
- 运行单元测试

### 前置依赖
```bash
# macOS
brew install cmake conan nlohmann-json googletest

# Ubuntu/Debian
sudo apt-get install cmake nlohmann-json3-dev libgtest-dev
pip install conan

# 注意：yaml-cpp 和 spdlog 由 iov-vehicle-tbox-framework 提供
```

### 编译步骤
```bash
cd /Users/hwyz_leo/Projects/open-iov/vehicle/tbox/iov-vehicle-tbox-prov

# 1. 安装 Conan 依赖
conan install . --output-folder=build --build=missing

# 2. 编译
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 3. 运行测试
./TboxProvTests
# 或
ctest --output-on-failure

# 4. 运行主程序
./TboxProvService
```

### 一键脚本
```bash
./scripts/build.sh test
```

## 方式二：Docker 交叉编译（部署用）

### 适用场景
- 生成部署到 TBOX 硬件的可执行文件
- 不想污染本地环境
- 保证编译环境一致性

### 前置依赖
- 仅需安装 Docker

### 编译步骤
```bash
cd /Users/hwyz_leo/Projects/open-iov/vehicle/tbox/iov-vehicle-tbox-prov

# 1. 构建 Docker 镜像（首次执行，后续会缓存）
docker build -t tbox-prov-builder -f Dockerfile.cross .

# 2. 编译并导出产物
mkdir -p output
docker run -v $(pwd)/output:/dest tbox-prov-builder

# 3. 查看产物
ls -la output/
# TboxProvService        # ARM64 Linux 可执行文件
# prov_config.yaml       # 配置文件示例
```

### 产物部署
```bash
# 将产物复制到 TBOX 设备
scp output/TboxProvService user@tbox-device:/usr/bin/
scp config/prov_config.yaml user@tbox-device:/etc/tbox/conf.d/prov.yaml
```

## 推荐工作流

```
开发阶段                          部署阶段
    │                                │
    ▼                                ▼
本地编译 ──► 开发 ──► 测试 ──► Docker交叉编译 ──► 部署到TBOX
  (快)      (快)     (快)         (产物正确)      (ARM64 Linux)
```

## 常见问题

### Q: 本地编译失败怎么办？
A: 检查依赖是否安装完整，参考 `conanfile.txt` 中的依赖列表

### Q: Docker 编译很慢？
A: 首次构建需要下载镜像和安装依赖，后续会使用缓存，速度会快很多

### Q: 两种方式可以混用吗？
A: 可以。本地编译用于开发测试，Docker 编译用于最终部署，互不影响

### Q: 如何清理编译产物？
```bash
# 清理本地编译产物
rm -rf build/*

# 清理 Docker 镜像
docker rmi tbox-prov-builder
```