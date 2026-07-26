#include "prov_context.h"
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

namespace tbox::prov {

std::string generate_request_id() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    
    std::ostringstream oss;
    oss << "prov-" << timestamp << "-" << dis(gen);
    return oss.str();
}

} // namespace tbox::prov
