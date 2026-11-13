#pragma once
#include "orbduel/core/Types.hpp"
#include <string>
struct ImDrawList;
namespace orbduel {
struct Palette {
    Rgba top, bottom, accent, secondary, stone;
};
const char *themeName(const std::string &);
Palette themePalette(const std::string &);
// 在视口内绘制纯表现背景，不访问或改变任何随机/物理状态。
void drawTheme(ImDrawList *, Rect viewport, const std::string &, float time, float density,
               const Palette *overrides = nullptr);
} // namespace orbduel
