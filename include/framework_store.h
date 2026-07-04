#pragma once

#include <string>
#include <optional>
#include <functional>
#include <cstdint>
#include <iostream>
#include <nlohmann/json.hpp>

namespace tbox {
namespace framework {

class Store {
public:
    Store(const std::string& service_name, const std::string& root_path);
    ~Store() = default;

    // 初始化存储
    bool initialize();

    // 保存数据到指定键
    template<typename T>
    bool save(const std::string& key, const T& value);

    // 从指定键加载数据
    template<typename T>
    std::optional<T> load(const std::string& key);

    // 检查键是否存在
    bool exists(const std::string& key);

    // 删除键
    bool remove(const std::string& key);

    // 获取存储根路径
    std::string get_root_path() const;

    // 获取服务存储路径
    std::string get_service_path() const;

private:
    std::string service_name_;
    std::string root_path_;
    std::string service_path_;

    // 内部文件操作
    bool write_to_file(const std::string& path, const std::string& data);
    std::string read_from_file(const std::string& path);
    bool file_exists(const std::string& path);
    bool create_directory(const std::string& path);
    bool atomic_write(const std::string& path, const std::string& data);
};

// 模板特化定义
template<>
inline bool Store::save<std::string>(const std::string& key, const std::string& value) {
    std::string path = service_path_ + "/" + key + ".json";
    nlohmann::json j;
    j["type"] = "string";
    j["value"] = value;
    return atomic_write(path, j.dump(4));
}

template<>
inline std::optional<std::string> Store::load<std::string>(const std::string& key) {
    std::string path = service_path_ + "/" + key + ".json";
    if (!file_exists(path)) {
        return std::nullopt;
    }
    try {
        std::string data = read_from_file(path);
        nlohmann::json j = nlohmann::json::parse(data);
        if (j["type"] == "string") {
            return j["value"].get<std::string>();
        }
        return std::nullopt;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load key " << key << ": " << e.what() << std::endl;
        return std::nullopt;
    }
}

} // namespace framework
} // namespace tbox