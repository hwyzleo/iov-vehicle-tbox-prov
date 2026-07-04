#include "framework_store.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <random>
#include <iomanip>
#include <nlohmann/json.hpp>

namespace tbox {
namespace framework {

Store::Store(const std::string& service_name, const std::string& root_path)
    : service_name_(service_name), root_path_(root_path) {
    service_path_ = root_path_ + "/" + service_name_;
}

bool Store::initialize() {
    try {
        if (!create_directory(root_path_)) {
            return false;
        }
        if (!create_directory(service_path_)) {
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Store initialization failed: " << e.what() << std::endl;
        return false;
    }
}

bool Store::exists(const std::string& key) {
    std::string path = service_path_ + "/" + key + ".json";
    return file_exists(path);
}

bool Store::remove(const std::string& key) {
    std::string path = service_path_ + "/" + key + ".json";
    try {
        return std::filesystem::remove(path);
    } catch (const std::exception& e) {
        std::cerr << "Failed to remove key " << key << ": " << e.what() << std::endl;
        return false;
    }
}

std::string Store::get_root_path() const {
    return root_path_;
}

std::string Store::get_service_path() const {
    return service_path_;
}

bool Store::write_to_file(const std::string& path, const std::string& data) {
    try {
        std::ofstream file(path);
        if (!file.is_open()) {
            return false;
        }
        file << data;
        file.close();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to write to file: " << e.what() << std::endl;
        return false;
    }
}

std::string Store::read_from_file(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            return "";
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    } catch (const std::exception& e) {
        std::cerr << "Failed to read from file: " << e.what() << std::endl;
        return "";
    }
}

bool Store::file_exists(const std::string& path) {
    return std::filesystem::exists(path);
}

bool Store::create_directory(const std::string& path) {
    try {
        if (!std::filesystem::exists(path)) {
            return std::filesystem::create_directories(path);
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to create directory: " << e.what() << std::endl;
        return false;
    }
}

bool Store::atomic_write(const std::string& path, const std::string& data) {
    try {
        // 生成临时文件名
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);
        std::stringstream ss;
        ss << path << ".tmp." << std::hex << std::setfill('0');
        for (int i = 0; i < 8; ++i) {
            ss << dis(gen);
        }
        std::string temp_path = ss.str();

        // 写入临时文件
        std::ofstream temp_file(temp_path, std::ios::binary);
        if (!temp_file.is_open()) {
            return false;
        }
        temp_file << data;
        temp_file.flush();
        temp_file.close();

        // 原子重命名
        std::filesystem::rename(temp_path, path);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Atomic write failed: " << e.what() << std::endl;
        return false;
    }
}



} // namespace framework
} // namespace tbox