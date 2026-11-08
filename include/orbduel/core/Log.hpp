#pragma once

// 日志：控制台 + 屏幕内环形缓冲（最近 N 行）。
// 内容错误走 log::warn / log::error，不抛异常（热重载不能崩）。

#include <cstddef>
#include <string>
#include <vector>

namespace orbduel {
namespace log {

void info(const std::string& message);
void warn(const std::string& message);
void error(const std::string& message);

// 清空屏幕缓冲（不清控制台）。
void clear();

// 最近 lines 行（最多 16 行），供 render/ 的屏幕控制台绘制。
std::vector<std::string> recentLines(std::size_t lines = 8);

} // namespace log
} // namespace orbduel
