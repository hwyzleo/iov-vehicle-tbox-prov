//
// TBOX-PROV-DSN-CR-008 §11.1: ProvApplication 单元测试。
// 覆盖信号集合、initialize 各阶段成功/失败与逆序清理、execute 退出、cleanup 幂等。
//

#include <gtest/gtest.h>
#include "prov_application.h"
#include "prov_service.h"
#include "framework_store.h"
#include "config.h"
#include "application.h"

#include <csignal>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

using namespace tbox::prov;

namespace {
/// 暴露 protected 钩子便于单测直接驱动生命周期阶段（不经过 run() 的信号安装）。
class TestableProvApplication : public ProvApplication {
public:
    using ProvApplication::ProvApplication;
    std::string serviceName() const { return getServiceName(); }
    std::vector<int> gracefulSigs() const { return gracefulSignals(); }
    std::vector<int> ignoredSigs() const { return ignoredSignals(); }
    std::vector<int> fatalSigs() const { return fatalSignals(); }
    bool doLoadConfig(const std::string& root) {
        return hwyz::config::ConfigManager::instance().load("prov", root)
               == hwyz::config::ConfigError::kOk;
    }
    bool doInitialize() { return initialize(); }
    void doCleanup() { cleanup(); }
    int doExecute() { return execute(); }
    void doRequestShutdown() { requestShutdown(); }
};

std::string makeConfig(const std::string& dir,
                       const std::string& store_root,
                       const std::string& socket_path) {
    std::filesystem::create_directories(std::string(dir) + "/conf.d");
    {
        std::ofstream f(std::string(dir) + "/common.yaml");
        f << "common:\n  store:\n    root: \"" << store_root << "\"\n"
          << "  log:\n    level: info\n";
    }
    {
        std::ofstream f(std::string(dir) + "/conf.d/prov.yaml");
        f << "prov:\n  ipc:\n    socket_path: \"" << socket_path << "\"\n"
          << "ecu:\n  uid: \"00000000000000000000000000000001\"\n";
    }
    return dir;
}
} // namespace

class ProvApplicationTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/prov_app_test_" +
            std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(test_dir_);
        store_dir_ = test_dir_ + "/store";
        socket_path_ = test_dir_ + "/test.sock";
    }
    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }
    std::string test_dir_;
    std::string store_dir_;
    std::string socket_path_;
};

// ============================================================
// 信号集合与服务标识
// ============================================================

TEST_F(ProvApplicationTest, ServiceNameIsProv) {
    TestableProvApplication app;
    EXPECT_EQ(app.serviceName(), "prov");
}

TEST_F(ProvApplicationTest, GracefulSignalsIncludeSighup) {
    TestableProvApplication app;
    auto sigs = app.gracefulSigs();
    // CR-008 §6: SIGINT/SIGTERM/SIGHUP 均 graceful
    EXPECT_NE(std::find(sigs.begin(), sigs.end(), SIGINT), sigs.end());
    EXPECT_NE(std::find(sigs.begin(), sigs.end(), SIGTERM), sigs.end());
    EXPECT_NE(std::find(sigs.begin(), sigs.end(), SIGHUP), sigs.end());
}

TEST_F(ProvApplicationTest, IgnoredSignalsIncludeSigpipe) {
    TestableProvApplication app;
    auto sigs = app.ignoredSigs();
    EXPECT_NE(std::find(sigs.begin(), sigs.end(), SIGPIPE), sigs.end());
}

TEST_F(ProvApplicationTest, SignalSetsValidAndMutuallyExclusive) {
    TestableProvApplication app;
    auto g = app.gracefulSigs();
    auto f = app.fatalSigs();
    auto i = app.ignoredSigs();
    // 集合合法、去重、无跨集合冲突
    EXPECT_TRUE(hwyz::Application::validateSignalSets(g, f, i).empty());
    // SIGKILL/SIGSTOP 不得出现
    for (int s : g) { EXPECT_NE(s, SIGKILL); EXPECT_NE(s, SIGSTOP); }
    for (int s : f) { EXPECT_NE(s, SIGKILL); EXPECT_NE(s, SIGSTOP); }
}

// ============================================================
// initialize 成功与逆序清理
// ============================================================

TEST_F(ProvApplicationTest, InitializeSuccessStartsIpc) {
    makeConfig(test_dir_, store_dir_, socket_path_);
    TestableProvApplication app;
    ASSERT_TRUE(app.doLoadConfig(test_dir_));
    ASSERT_TRUE(app.doInitialize());
    // IPC server 启动后应创建 socket 文件
    EXPECT_TRUE(std::filesystem::exists(socket_path_));

    app.doCleanup();
    // cleanup 后 socket 路径不得残留（CR-008 §4.3）
    EXPECT_FALSE(std::filesystem::exists(socket_path_));
}

TEST_F(ProvApplicationTest, CleanupIsIdempotent) {
    makeConfig(test_dir_, store_dir_, socket_path_);
    TestableProvApplication app;
    ASSERT_TRUE(app.doLoadConfig(test_dir_));
    ASSERT_TRUE(app.doInitialize());

    app.doCleanup();
    // 重复 cleanup 必须安全（幂等，不重复业务写入）
    EXPECT_NO_THROW(app.doCleanup());
    EXPECT_NO_THROW(app.doCleanup());
    EXPECT_FALSE(std::filesystem::exists(socket_path_));
}

TEST_F(ProvApplicationTest, InitializeStoreFailureReturnsFalse) {
    // store_root 指向不可创建目录的路径（/dev/null 是文件，其下无法建目录）
    makeConfig(test_dir_, "/dev/null/cannot_create_store", socket_path_);
    TestableProvApplication app;
    ASSERT_TRUE(app.doLoadConfig(test_dir_));
    EXPECT_FALSE(app.doInitialize());
    // store 失败时不得启动 IPC，无 socket 残留
    EXPECT_FALSE(std::filesystem::exists(socket_path_));
}

TEST_F(ProvApplicationTest, InitializeIpcBindFailureRollsBack) {
    // socket 路径位于 /dev/null（文件）之下，bind 必然失败（ENOTDIR）；
    // 验证 IPC start 失败后 initialize 返回 false 且无半初始化资源、cleanup 幂等。
    makeConfig(test_dir_, store_dir_, "/dev/null/cannot_bind.sock");
    TestableProvApplication app;
    ASSERT_TRUE(app.doLoadConfig(test_dir_));
    EXPECT_FALSE(app.doInitialize());
    // 失败后 cleanup 必须安全（幂等）
    EXPECT_NO_THROW(app.doCleanup());
}

// ============================================================
// execute 在收到停机请求后及时返回
// ============================================================

TEST_F(ProvApplicationTest, ExecuteReturnsAfterShutdownRequest) {
    makeConfig(test_dir_, store_dir_, socket_path_);
    TestableProvApplication app;
    ASSERT_TRUE(app.doLoadConfig(test_dir_));
    ASSERT_TRUE(app.doInitialize());

    // 异步触发停机；execute 不依赖 EINTR，应在 poll 周期内返回
    std::thread([&app]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        app.doRequestShutdown();
    }).detach();

    auto start = std::chrono::steady_clock::now();
    int rc = app.doExecute();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    EXPECT_EQ(rc, 0);
    // 默认 poll 100ms + 触发延迟 150ms，应在 1s 内返回（不依赖 EINTR）
    EXPECT_LT(elapsed, 1000);

    app.doCleanup();
}
