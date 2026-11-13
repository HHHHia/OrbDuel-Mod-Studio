#include "orbduel/render/Themes.hpp"
#include <algorithm>
#include <cmath>
#include <imgui.h>
namespace orbduel {
namespace {
ImU32 c(Rgba a) {
    return IM_COL32(a.r, a.g, a.b, a.a);
}
} // namespace
const char *themeName(const std::string &t) {
    if (t == "dungeon")
        return "地下城";
    if (t == "neon")
        return "霓虹都市";
    if (t == "lava")
        return "熔岩遗迹";
    if (t == "ice")
        return "冰晶圣殿";
    return "星空";
}
Palette themePalette(const std::string &t) {
    if (t == "dungeon")
        return {{23, 20, 29, 255},
                {38, 29, 36, 255},
                {245, 183, 95, 255},
                {190, 115, 110, 255},
                {64, 53, 64, 255}};
    if (t == "neon")
        return {{12, 17, 40, 255},
                {29, 18, 47, 255},
                {60, 229, 236, 255},
                {245, 93, 210, 255},
                {42, 44, 78, 255}};
    if (t == "lava")
        return {
            {26, 17, 25, 255}, {48, 20, 25, 255}, {255, 137, 69, 255}, {255, 76, 93, 255}, {64, 36, 42, 255}};
    if (t == "ice")
        return {{17, 31, 48, 255},
                {28, 49, 66, 255},
                {135, 229, 255, 255},
                {175, 172, 255, 255},
                {53, 81, 103, 255}};
    return {
        {12, 17, 38, 255}, {22, 25, 53, 255}, {159, 167, 255, 255}, {104, 221, 215, 255}, {41, 47, 78, 255}};
}
void drawTheme(ImDrawList *d, Rect r, const std::string &t, float time, float density,
               const Palette *overrides) {
    auto p = overrides ? *overrides : themePalette(t);
    auto point = [&](float x, float y) { return ImVec2(r.x + x * r.w, r.y + y * r.h); };
    auto A = point(0, 0), B = point(1, 1);
    d->PushClipRect(A, B, true);
    d->AddRectFilledMultiColor(A, B, c(p.top), c(p.top), c(p.bottom), c(p.bottom));
    int n = int(std::max(0.f, std::min(1.f, density)) * 100);
    if (t == "cosmos") {
        for (int j = 8; j > 0; --j) {
            float alpha = 3.f;
            d->AddCircleFilled(point(.7f, .3f), r.h * (.1f + j * .035f), IM_COL32(104, 76, 180, int(alpha)),
                               64);
        }
        for (int i = 0; i < n + 20; ++i) {
            float x = std::fmod(i * .6180339887f + .03f, 1.f), y = std::fmod(i * .41421356f + .11f, 1.f);
            int a = int(80 + 80 * (.5f + .5f * std::sin(time * .5f + i)));
            d->AddCircleFilled(point(x, y), i % 9 == 0 ? 1.7f : .8f, IM_COL32(184, 201, 255, a));
        }
        d->AddCircle(point(.78f, .24f), r.h * .1f, IM_COL32(154, 135, 240, 35), 64, 1);
        d->AddEllipse(point(.78f, .24f), {r.h * .15f, r.h * .035f}, IM_COL32(154, 135, 240, 55), -.3f, 64, 1);
    } else if (t == "dungeon") {
        float tile = r.w / 18;
        for (int row = 0; row < 16; ++row)
            for (int col = -1; col < 20; ++col) {
                float x = r.x + col * tile + (row % 2) * tile * .5f, y = r.y + row * tile * .55f;
                d->AddRect({x + 2, y + 2}, {x + tile - 2, y + tile * .55f - 2}, IM_COL32(120, 98, 113, 25),
                           3);
            }
        for (float x : {.06f, .94f})
            for (float y : {.12f, .88f}) {
                auto a = point(x, y);
                float flame = 5 + std::sin(time * 6 + x + y) * 1.5f;
                for (int k = 6; k > 0; --k)
                    d->AddCircleFilled(a, flame + k * 5.f, IM_COL32(255, 165, 69, 5), 24);
                d->AddCircleFilled(a, flame, IM_COL32(255, 197, 112, 220), 12);
            }
    } else if (t == "neon") {
        for (int i = 0; i < 23; ++i) {
            float x = r.x + r.w * i / 22.f;
            d->AddLine({x, r.y}, {x, r.y + r.h}, IM_COL32(75, 164, 200, 18));
        }
        for (int i = 0; i < 15; ++i) {
            float y = r.y + r.h * i / 14.f;
            d->AddLine({r.x, y}, {r.x + r.w, y}, IM_COL32(145, 81, 213, 20));
        }
        for (int i = 0; i < 24; ++i) {
            float w = r.w / 27, h = r.h * (.04f + float((i * 13) % 7) * .012f), x = r.x + i * r.w / 24;
            d->AddRectFilled({x, r.y + r.h - h}, {x + w, r.y + r.h}, IM_COL32(45, 37, 83, 140));
            d->AddLine({x, r.y + r.h - h}, {x + w, r.y + r.h - h}, IM_COL32(232, 74, 205, 70), 2);
        }
        float y = std::fmod(time * .03f, 1.f);
        d->AddLine(point(0, y), point(1, y), IM_COL32(65, 229, 236, 25));
    } else if (t == "lava") {
        for (int i = 0; i < 10; ++i) {
            ImVec2 prev = point(float(i) / 9, 0);
            for (int j = 1; j <= 9; ++j) {
                auto next = point(float(i) / 9 + std::sin(i * 3.f + j * 1.7f) * .035f, j / 9.f);
                d->AddLine(prev, next, IM_COL32(240, 80, 40, 20), 7);
                d->AddLine(prev, next, IM_COL32(255, 123, 55, 65), 1);
                prev = next;
            }
        }
        for (int i = 0; i < n / 2; ++i) {
            float x = std::fmod(i * .618f, 1.f), y = 1 - std::fmod(i * .37f + time * .025f, 1.f);
            d->AddCircleFilled(point(x, y), 1.5f, IM_COL32(255, 170, 80, 100));
        }
    } else {
        for (int i = 0; i < 18; ++i) {
            float x = std::fmod(i * .618f, 1.f), y = std::fmod(i * .414f, 1.f);
            auto a = point(x, y);
            float s = r.h * (.018f + .005f * (i % 3));
            d->AddQuadFilled({a.x, a.y - s * 2}, {a.x + s, a.y}, {a.x, a.y + s * 2}, {a.x - s, a.y},
                             IM_COL32(140, 210, 255, 13));
            d->AddLine({a.x, a.y - s * 2}, {a.x + s, a.y}, IM_COL32(160, 235, 255, 45));
        }
        for (int i = 0; i < n / 2; ++i) {
            auto a = point(std::fmod(i * .618f + time * .005f, 1.f), std::fmod(i * .414f + time * .02f, 1.f));
            d->AddLine({a.x - 2, a.y}, {a.x + 2, a.y}, IM_COL32(190, 235, 255, 75));
            d->AddLine({a.x, a.y - 2}, {a.x, a.y + 2}, IM_COL32(190, 235, 255, 75));
        }
    }
    d->PopClipRect();
}
} // namespace orbduel
