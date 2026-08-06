// TBOX-PROV-DSN-CR-010 §13: 配置资产验收测试
//
// 验证 PROV 仓库配置目录分层、默认模板边界、schema 存在性与 CMake install 规则，
// 防止调试文件/正式 common/测试 UID-SN 实例值进入发布包。
//
// 测试从 build/ 目录运行，current_path().parent_path() 指向项目根。

#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::filesystem::path project_root() {
    return std::filesystem::current_path().parent_path();
}

std::string read_text(const std::filesystem::path& p) {
    std::ifstream f(p);
    return std::string((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
}

class ConfigAssetsTest : public ::testing::Test {};

// §5: prov.default.yaml 不得包含顶层 common:（由 BUILD common.yaml 唯一提供）
TEST_F(ConfigAssetsTest, DefaultTemplateHasNoTopLevelCommon) {
    auto path = project_root() / "config" / "prov.default.yaml";
    ASSERT_TRUE(std::filesystem::exists(path)) << path;
    YAML::Node n = YAML::LoadFile(path.string());
    ASSERT_TRUE(n.IsMap());
    EXPECT_FALSE(n["common"])
        << "prov.default.yaml must not contain top-level 'common:'";
}

// §5: prov.default.yaml 不得包含 prov.uid / prov.sn 实例值
TEST_F(ConfigAssetsTest, DefaultTemplateHasNoTestUidSn) {
    auto path = project_root() / "config" / "prov.default.yaml";
    YAML::Node n = YAML::LoadFile(path.string());
    ASSERT_TRUE(n["prov"]);
    EXPECT_FALSE(n["prov"]["uid"])
        << "prov.uid must not appear in default template";
    EXPECT_FALSE(n["prov"]["sn"])
        << "prov.sn must not appear in default template";
}

// §5: prov.default.yaml 字段结构合法
TEST_F(ConfigAssetsTest, DefaultTemplateHasRequiredFields) {
    auto path = project_root() / "config" / "prov.default.yaml";
    YAML::Node n = YAML::LoadFile(path.string());
    ASSERT_TRUE(n["prov"]["ipc"]["socket_path"]);
    EXPECT_TRUE(n["prov"]["ipc"]["socket_path"].IsScalar());
    ASSERT_TRUE(n["storage"]["enable_write_protection"]);
    ASSERT_TRUE(n["storage"]["max_retry_count"]);
}

// §5: prov.default.yaml 不得含 Orin/VIN/证书/Token 等设备实例或秘密字段
TEST_F(ConfigAssetsTest, DefaultTemplateHasNoDeviceSecrets) {
    auto path = project_root() / "config" / "prov.default.yaml";
    YAML::Node n = YAML::LoadFile(path.string());
    EXPECT_FALSE(n["vin"]);
    EXPECT_FALSE(n["certificate"]);
    EXPECT_FALSE(n["token"]);
    EXPECT_FALSE(n["kek"]);
}

// §3: config/dev/ 调试配置存在
TEST_F(ConfigAssetsTest, DevConfigExists) {
    EXPECT_TRUE(std::filesystem::exists(project_root() / "config" / "dev" / "common.yaml"));
    EXPECT_TRUE(std::filesystem::exists(project_root() / "config" / "dev" / "prov.yaml"));
}

// §3/§7: config/dev/prov.yaml 含明确测试占位值（非真实设备值、非空）
TEST_F(ConfigAssetsTest, DevProvHasTestPlaceholders) {
    auto path = project_root() / "config" / "dev" / "prov.yaml";
    YAML::Node n = YAML::LoadFile(path.string());
    ASSERT_TRUE(n["prov"]["sn"]);
    ASSERT_TRUE(n["ecu"]["uid"]);
    EXPECT_FALSE(n["prov"]["sn"].as<std::string>().empty());
    EXPECT_FALSE(n["ecu"]["uid"].as<std::string>().empty());
}

// §8: config/schema/prov.schema.yaml 存在且结构合法
TEST_F(ConfigAssetsTest, SchemaExistsAndValid) {
    auto path = project_root() / "config" / "schema" / "prov.schema.yaml";
    ASSERT_TRUE(std::filesystem::exists(path)) << path;
    YAML::Node n = YAML::LoadFile(path.string());
    ASSERT_TRUE(n.IsMap());
    EXPECT_EQ(n["schema_version"].as<std::string>(), "1.0");
    EXPECT_TRUE(n["root"]["forbidden_top_level_keys"]);
    EXPECT_TRUE(n["fail_closed"]);
    EXPECT_TRUE(n["compatibility"]);
}

// §8: schema 声明 prov.uid/prov.sn 为 test_only
TEST_F(ConfigAssetsTest, SchemaMarksUidSnTestOnly) {
    auto path = project_root() / "config" / "schema" / "prov.schema.yaml";
    YAML::Node n = YAML::LoadFile(path.string());
    ASSERT_TRUE(n["prov"]["fields"]["uid"]);
    EXPECT_TRUE(n["prov"]["fields"]["uid"]["test_only"].as<bool>());
    ASSERT_TRUE(n["prov"]["fields"]["sn"]);
    EXPECT_TRUE(n["prov"]["fields"]["sn"]["test_only"].as<bool>());
}

// §4: CMakeLists.txt 不得安装 config/common.yaml，不得递归安装 config/
// （逐行检查非注释行，避免注释中的示例文本误匹配）
TEST_F(ConfigAssetsTest, CMakeDoesNotInstallDevOrCommon) {
    auto path = project_root() / "CMakeLists.txt";
    ASSERT_TRUE(std::filesystem::exists(path));
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        size_t first_non_ws = line.find_first_not_of(" \t");
        if (first_non_ws == std::string::npos) continue;
        if (line[first_non_ws] == '#') continue;  // 跳过注释行
        EXPECT_EQ(line.find("install(FILES config/common.yaml"), std::string::npos)
            << "PROV must not install /etc/tbox/common.yaml (owned by BUILD)";
        EXPECT_EQ(line.find("install(DIRECTORY config/"), std::string::npos)
            << "PROV must not recursively install config/ (dev files must not ship)";
    }
}

// §4: CMakeLists.txt 仍正确安装 prov.default.yaml -> conf.d/prov.yaml
TEST_F(ConfigAssetsTest, CMakeInstallsDefaultTemplate) {
    auto path = project_root() / "CMakeLists.txt";
    std::string content = read_text(path);
    EXPECT_NE(content.find("install(FILES config/prov.default.yaml"), std::string::npos);
    EXPECT_NE(content.find("RENAME prov.yaml"), std::string::npos);
    EXPECT_NE(content.find("tbox/conf.d"), std::string::npos);
}

} // namespace
