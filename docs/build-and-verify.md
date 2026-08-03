# TBOX PROV 编译与验证指南

## 编译方式对比

| 对比项 | 本机编译 | 交叉编译 (aarch64 / Orin) |
|--------|----------|---------------------------|
| **入口** | 本仓库 `./scripts/build.sh` | `iov-vehicle-tbox-build` 编排器 |
| **命令** | `./scripts/build.sh` | `python3 -m tbox_build build --service prov` |
| **编译环境** | 当前主机 (macOS/Linux) | Docker (linux/arm64) + Orin 工具链 |
| **生成产物** | 主机可执行文件 | aarch64 Linux 可执行文件 |
| **用途** | 本地开发、调试、单元测试 | 出 Orin 产物、打包、部署 |
| **工具链/依赖** | Conan（本机 profile） | build 项目的 `orin-aarch64.cmake` + sysroot + `TBOX_DEP_STAGING` |
| **能否跑测试** | 能 | 交叉产物不在本机运行 |

> 交叉编译**不在本仓库进行**。framework、prov 等所有服务的 aarch64 构建都由
> `iov-vehicle-tbox-build` 统一编排（详见"方式二"）。本仓库的 `build.sh`
> 只负责本机开发与单元测试。

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
./tbox_prov
```

### 一键脚本
```bash
./scripts/build.sh            # 完整构建（Release + 测试）
./scripts/build.sh --no-test  # 仅构建，跳过测试
./scripts/build.sh --clean    # 清理后重新构建
./scripts/build.sh --debug    # Debug 模式构建
./scripts/build.sh --help     # 查看全部选项
```

## 方式二：交叉编译（统一在 iov-vehicle-tbox-build 进行）

### 适用场景
- 生成部署到 TBOX（aarch64 / NVIDIA Orin）硬件的可执行文件
- 打包、部署

### 机制说明
交叉编译由 `iov-vehicle-tbox-build` 编排器负责，它直接对本仓库的
`CMakeLists.txt` 调用 `cmake`，并注入：
- 工具链：`cmake/toolchains/orin-aarch64.cmake`（`orin-release` preset）
- sysroot：`sysroots/orin-r35.3.1`
- 目标依赖：framework 的 SDK staging、`TBOX_DEP_STAGING` 中的 nlohmann-json

本仓库 CMakeLists.txt 因此不硬编码编译器（见 CR-009 注释），编译器/前缀均由
build 项目的 toolchain/preset 注入。prov 已登记在 `manifests/services.yaml`
（`preset: orin-release`，依赖 framework + nlohmann-json）。

### 编译步骤
```bash
cd ../iov-vehicle-tbox-build

# 1. 导入并校验 Orin sysroot（首次）
python3 -m tbox_build sysroot import
python3 -m tbox_build validate

# 2. 交叉编译 prov（会先按依赖顺序构建 framework）
python3 -m tbox_build build --platform orin --profile release --service prov
#   或在 Docker 中构建整套：
./ci/build-in-docker.sh --set tbox-orin-minimal

# 3. 打包 / 验证
python3 -m tbox_build package
python3 -m tbox_build verify
```

> ℹ️ 早期的 `Dockerfile.cross`、`toolchain-aarch64-linux-gnu.cmake` 已移除；
> 本仓库也不再提供 `build.sh --cross` 旁路。交叉编译统一走 tbox-build，
> 避免出现多套不一致的交叉编译路径。

## 推荐工作流

```
开发阶段（本仓库）                       部署阶段（tbox-build 项目）
    │                                        │
    ▼                                        ▼
本机编译 ──► 开发 ──► 单元测试 ──► tbox_build build ──► package ──► 部署到 Orin
build.sh                          (aarch64 交叉编译)              (aarch64 Linux)
```

## 常见问题

### Q: 本机编译失败怎么办？
A: 检查依赖是否安装完整，参考 `conanfile.txt` 中的依赖列表。

### Q: 为什么 build.sh 没有交叉编译选项？
A: 有意为之。交叉编译集中在 `iov-vehicle-tbox-build`（统一工具链 / sysroot /
   依赖 staging），本仓库 build.sh 只做本机开发与单测，避免多套路径不一致。

### Q: 交叉编译报找不到 framework？
A: 需先由 tbox-build 构建并 staging framework（prov 已声明 `service_dependencies:
   [framework]`，编排器会按依赖顺序先建 framework）。确认 sysroot 已 `import` 且
   `validate` 通过。

### Q: 如何清理本机编译产物？
```bash
rm -rf build/*
```