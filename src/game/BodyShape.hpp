#pragma once

// 程序化皮肤：把角色画成几何体，而不是贴图。
// 形状由 CharacterDef.skin.shape 决定；不是程序化形状时返回 false，调用方继续画贴图。
// 比例放在代码里而不是 JSON：跟着 hitbox 半径缩放，调形状只改这几个常量。

#include <algorithm>
#include <cmath>

#include "orbduel/content/DataModel.hpp"
#include "orbduel/render/Renderer.hpp"

namespace orbduel {

// 瘦长矛头：尖端在 1.35×半径，尾巴在 -0.9×半径，尾巴半宽 0.72×半径。
constexpr float kDartTip = 1.35f;
constexpr float kDartTail = -0.9f;
constexpr float kDartHalfWidth = 0.72f;

// 画完返回 true；返回 false 表示这条皮肤该走贴图。
inline bool drawShapeSkin(Renderer& renderer, const SkinDef& skin, Vec2 center, float radius,
                          float angleRad, Rgba tint) {
    if (skin.shape != "triangle") {
        return false;
    }

    const Vec2 dir = fromAngle(angleRad);
    const Vec2 side{-dir.y, dir.x};
    const Vec2 back = center + dir * (radius * kDartTail);
    const Vec2 points[3] = {center + dir * (radius * kDartTip),
                            back + side * (radius * kDartHalfWidth),
                            back - side * (radius * kDartHalfWidth)};
    renderer.drawConvexPoly(points, 3, tint);

    // 描边亮一档，小尺寸下否则看不出边
    const Rgba edge{static_cast<unsigned char>(std::min(255, static_cast<int>(tint.r) + 70)),
                    static_cast<unsigned char>(std::min(255, static_cast<int>(tint.g) + 70)),
                    static_cast<unsigned char>(std::min(255, static_cast<int>(tint.b) + 70)),
                    tint.a};
    for (int i = 0; i < 3; ++i) {
        renderer.drawLine(points[i], points[(i + 1) % 3], 2.0f, edge);
    }
    return true;
}

// 尖角离圆心多远；蓄力虚线从这里出发才是「尖角指的方向」。
inline float skinTipDistance(const SkinDef& skin, float radius) {
    return skin.shape == "triangle" ? radius * kDartTip : radius;
}

} // namespace orbduel
