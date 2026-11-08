#pragma once

// 基础类型：不依赖 SDL2，physics / content / match 都用这一套。
// 与 SDL2 的转换只在 render/ 边界发生（见 ADR-006）。

#include <algorithm>
#include <cmath>

namespace orbduel {

constexpr float kPi = 3.14159265358979323846f;

// 二维向量。默认零向量，不是未初始化。
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vec2 operator-(Vec2 v) { return {-v.x, -v.y}; }
inline Vec2 operator*(Vec2 v, float s) { return {v.x * s, v.y * s}; }
inline Vec2 operator*(float s, Vec2 v) { return {v.x * s, v.y * s}; }
inline Vec2 operator/(Vec2 v, float s) { return {v.x / s, v.y / s}; }
inline Vec2& operator+=(Vec2& a, Vec2 b) { a.x += b.x; a.y += b.y; return a; }
inline Vec2& operator-=(Vec2& a, Vec2 b) { a.x -= b.x; a.y -= b.y; return a; }
inline Vec2& operator*=(Vec2& a, float s) { a.x *= s; a.y *= s; return a; }

inline float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline float lengthSq(Vec2 v) { return dot(v, v); }
inline float lengthOf(Vec2 v) { return std::sqrt(lengthSq(v)); }

// 零长度输入返回零向量，不产生 NaN。
inline Vec2 normalized(Vec2 v) {
    float len = lengthOf(v);
    if (len <= 1e-6f) { return {0.0f, 0.0f}; }
    return {v.x / len, v.y / len};
}

inline Vec2 fromAngle(float radians) { return {std::cos(radians), std::sin(radians)}; }
inline float angleOf(Vec2 v) { return std::atan2(v.y, v.x); }

// 把向量旋转 radians（顺时针为正，因为 y 轴向下）。
inline Vec2 rotated(Vec2 v, float radians) {
    float c = std::cos(radians);
    float s = std::sin(radians);
    return {v.x * c - v.y * s, v.x * s + v.y * c};
}

// 四元组颜色，0-255。
struct Rgba {
    unsigned char r = 255;
    unsigned char g = 255;
    unsigned char b = 255;
    unsigned char a = 255;
};

// 轴对齐矩形：左上角 + 宽高。
struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    float left() const { return x; }
    float top() const { return y; }
    float right() const { return x + w; }
    float bottom() const { return y + h; }
    Vec2 center() const { return {x + w * 0.5f, y + h * 0.5f}; }
    Vec2 size() const { return {w, h}; }
};

inline float degToRad(float degrees) { return degrees * (kPi / 180.0f); }
inline float radToDeg(float radians) { return radians * (180.0f / kPi); }

inline float clampf(float value, float low, float high) {
    return std::max(low, std::min(high, value));
}

} // namespace orbduel
