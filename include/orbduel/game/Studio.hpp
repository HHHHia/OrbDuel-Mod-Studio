#pragma once

// 工作台入口。
//
// 不变量：
//   1. 物理步数只由 Clock 决定
//   2. 场景切换不重建 ContentLoader（重载只发生在显式操作）
//   3. --headless / --autotest 不创建窗口

#include "orbduel/game/Options.hpp"

namespace orbduel {

// 加载内容与模组，运行概览、编辑器、模组工坊和自动对战。
// 返回进程退出码：0 正常 / 2 初始化失败 / 3 内容致命错误。
int runStudio(const Options &options);

} // namespace orbduel
