#pragma once

// 圆-圆、圆-矩形的解析碰撞。全部是纯函数，不碰事件、不碰 Arena。
// 约定：法线由 a 指向 b；depth 是重叠深度（>=0）。

#include "orbduel/core/Types.hpp"
#include "orbduel/physics/Body.hpp"

namespace orbduel {

struct Contact {
    bool hit = false;
    Vec2 point{};          // 接触点（世界坐标）
    Vec2 normal{};         // 单位法线，从 a 指向 b
    float depth = 0.0f;    // 重叠深度
    float relSpeed = 0.0f; // 沿法线的接近速度，正数表示正在靠近
};

// 两圆碰撞。用 pos 与 radius，不考虑 hitbox.offset（那是表现层的事）。
Contact collideCircles(const Body& a, const Body& b);

// 圆 vs 轴对齐矩形。out.normal 指向把圆推出去的方向，out.depth 为嵌入深度。
bool collideCircleRect(const Body& body, const Rect& rect, Contact& out);

// 球球响应：位置按质量比例分离，速度沿法线交换（恢复系数取 min）。
void resolveCircles(Body& a, Body& b, const Contact& contact);

// 球 vs 矩形响应：推出 + 反射法线方向速度。
void resolveCircleRect(Body& body, const Rect& rect, const Contact& contact, float restitution);

// 球 vs 静态圆（障碍）。
bool collideCircleStatic(const Body& body, Vec2 center, float radius, Contact& out);
void resolveCircleStatic(Body& body, Vec2 center, float radius, const Contact& contact, float restitution);

} // namespace orbduel
