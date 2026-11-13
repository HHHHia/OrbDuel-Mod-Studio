#include "orbduel/version.hpp"

namespace orbduel {

// 版本号在 CMakeLists.txt 的 project(VERSION ...) 里维护，这里保持同步。
const char* versionString() {
    return "0.3.0-roster";
}

} // namespace orbduel
