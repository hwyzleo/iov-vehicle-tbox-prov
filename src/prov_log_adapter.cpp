#include "prov_log_adapter.h"

namespace tbox::prov {

bool ProvLogAdapter::s_initialized = false;

tbox::fw::log::InitResult ProvLogAdapter::init(
    const std::string& service,
    const tbox::fw::log::LogConfig& config
) {
    auto result = tbox::fw::log::Logger::init(service, config);
    if (result.error == tbox::fw::log::LogError::kOk) {
        s_initialized = true;
    }
    return result;
}

tbox::fw::log::Logger ProvLogAdapter::service() {
    return tbox::fw::log::Logger::get("service");
}

tbox::fw::log::Logger ProvLogAdapter::vin() {
    return tbox::fw::log::Logger::get("vin");
}

tbox::fw::log::Logger ProvLogAdapter::binding() {
    return tbox::fw::log::Logger::get("binding");
}

tbox::fw::log::Logger ProvLogAdapter::rewrite() {
    return tbox::fw::log::Logger::get("rewrite");
}

tbox::fw::log::Logger ProvLogAdapter::vehicle_config() {
    return tbox::fw::log::Logger::get("vehicle_config");
}

tbox::fw::log::Logger ProvLogAdapter::production() {
    return tbox::fw::log::Logger::get("production");
}

tbox::fw::log::Logger ProvLogAdapter::ipc() {
    return tbox::fw::log::Logger::get("ipc");
}

} // namespace tbox::prov
