#include "data_models.h"

namespace tbox {
namespace prov {

// VIN格式约束的静态成员初始化
constexpr size_t VinConstraints::VIN_LENGTH;
constexpr char VinConstraints::EXCLUDED_CHARS[];
constexpr size_t VinConstraints::CHECK_DIGIT_POSITION;

} // namespace prov
} // namespace tbox