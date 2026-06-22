#include "vin_validator.h"
#include "data_models.h"
#include <algorithm>
#include <cctype>
#include <map>

namespace tbox {
namespace prov {

bool VinValidator::validate(const std::string& vin) {
    return check_length(vin) && check_charset(vin) && check_digit(vin);
}

std::string VinValidator::get_validation_error(const std::string& vin) {
    if (!check_length(vin)) {
        return "VIN长度必须为17位";
    }
    if (!check_charset(vin)) {
        return "VIN包含非法字符（I、O、Q不允许）";
    }
    if (!check_digit(vin)) {
        return "VIN校验位不正确";
    }
    return "";
}

bool VinValidator::check_length(const std::string& vin) {
    return vin.length() == VinConstraints::VIN_LENGTH;
}

bool VinValidator::check_charset(const std::string& vin) {
    for (char c : vin) {
        if (!std::isalnum(c)) {
            return false;
        }
        if (std::string(VinConstraints::EXCLUDED_CHARS).find(std::toupper(c)) != std::string::npos) {
            return false;
        }
    }
    return true;
}

bool VinValidator::check_digit(const std::string& vin) {
    if (vin.length() != VinConstraints::VIN_LENGTH) {
        return false;
    }
    
    char calculated = calculate_check_digit(vin);
    char provided = std::toupper(vin[VinConstraints::CHECK_DIGIT_POSITION - 1]);
    
    return calculated == provided;
}

char VinValidator::calculate_check_digit(const std::string& vin) {
    // VIN校验位计算算法
    static const std::map<char, int> transliteration = {
        {'A', 1}, {'B', 2}, {'C', 3}, {'D', 4}, {'E', 5}, {'F', 6}, {'G', 7}, {'H', 8},
        {'J', 1}, {'K', 2}, {'L', 3}, {'M', 4}, {'N', 5}, {'P', 7}, {'R', 9},
        {'S', 2}, {'T', 3}, {'U', 4}, {'V', 5}, {'W', 6}, {'X', 7}, {'Y', 8}, {'Z', 9},
        {'1', 1}, {'2', 2}, {'3', 3}, {'4', 4}, {'5', 5}, {'6', 6}, {'7', 7}, {'8', 8}, {'9', 9}, {'0', 0}
    };
    
    static const int weights[] = {8, 7, 6, 5, 4, 3, 2, 10, 0, 9, 8, 7, 6, 5, 4, 3, 2};
    
    int sum = 0;
    for (size_t i = 0; i < VinConstraints::VIN_LENGTH; ++i) {
        char c = std::toupper(vin[i]);
        auto it = transliteration.find(c);
        if (it == transliteration.end()) {
            return 'X'; // 错误
        }
        sum += it->second * weights[i];
    }
    
    int remainder = sum % 11;
    if (remainder == 10) {
        return 'X';
    }
    return '0' + remainder;
}

int VinValidator::char_to_value(char c) {
    c = std::toupper(c);
    if (std::isdigit(c)) {
        return c - '0';
    }
    if (std::isalpha(c)) {
        // 跳过I、O、Q
        if (c == 'I' || c == 'O' || c == 'Q') {
            return -1;
        }
        return c - 'A' + 1;
    }
    return -1;
}

} // namespace prov
} // namespace tbox