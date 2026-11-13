#pragma once

// SDL2 的唯一出口（ADR-001）。其他模块不许 include <SDL.h>（AGENTS 铁律 2）。

#include <SDL.h>
#include <SDL_ttf.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "orbduel/core/Types.hpp"

namespace orbduel {

inline SDL_Color toSdl(Rgba color) {
    return SDL_Color{color.r, color.g, color.b, color.a};
}
inline SDL_FPoint toSdl(Vec2 v) {
    return SDL_FPoint{v.x, v.y};
}

class Renderer {
  public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;

    // hidden=true 时不显示窗口（--selftest 用）。
    bool init(int width, int height, const char *title, bool hidden = false);
    void shutdown();
    bool isInitialized() const;

    // 每帧调用一次，清空 SDL 事件队列并记录本帧按下的键。返回 false 表示窗口被关闭。
    bool pumpEvents();
    bool wasKeyPressed(SDL_Scancode code) const;
    bool shouldClose() const;
    void requestClose();

    // 鼠标：窗口坐标（左上原点）。clicked = 本帧左键按下过一次。
    Vec2 mousePosition() const;
    bool wasMouseClicked() const;

    void beginFrame(Rgba clearColor);
    void endFrame();

    int width() const;
    int height() const;
    float fps() const;

    void drawCircle(Vec2 center, float radius, Rgba color);
    void drawCircleLines(Vec2 center, float radius, Rgba color);
    void drawRing(Vec2 center, float radius, float thickness, Rgba color);
    void drawLine(Vec2 from, Vec2 to, float thickness, Rgba color);
    void drawRect(Rect rect, Rgba color);
    void drawRectLines(Rect rect, float thickness, Rgba color);

    // 填充凸多边形，points 是世界坐标顶点（不自交），count >= 3。
    // 给程序化皮肤用：形状由数据决定，坐标由 game 算好。
    void drawConvexPoly(const Vec2 *points, int count, Rgba color);

    // rotationDeg 顺时针为正；scale 只影响绘制尺寸，不影响碰撞。
    void drawSprite(SDL_Texture *texture, Vec2 center, float rotationDeg, float scale, Rgba tint);

    // SDL2_ttf 需要外部字体文件；baseSize 是常用字号，其他字号按需缓存。
    bool loadFont(const std::string &ttfPath, int baseSize);
    // 主字体没有的字（例如中文）用这个字体；没加载成功就继续用主字体（会显示成方框）。
    bool loadFallbackFont(const std::string &ttfPath, int baseSize);
    bool hasFallbackFont() const;
    void drawText(const std::string &text, Vec2 position, int fontSize, Rgba color);
    int measureText(const std::string &text, int fontSize) const;

    // 给 AssetCache 用，别在业务代码里直接拿它画东西。
    SDL_Renderer *backend() const;

    // 把当前帧存成 PNG（调试 / 截图用）。
    bool saveScreenshot(const std::string &path);

  private:
    TTF_Font *fontFor(int fontSize, bool fallback) const;

    SDL_Window *window_ = nullptr;
    SDL_Renderer *renderer_ = nullptr;

    std::string fontPath_;
    int baseFontSize_ = 16;
    mutable std::unordered_map<int, TTF_Font *> fonts_;

    std::string fallbackFontPath_;
    int fallbackBaseFontSize_ = 16;
    mutable std::unordered_map<int, TTF_Font *> fallbackFonts_;

    std::vector<SDL_Scancode> pressedKeys_;
    Vec2 mousePosition_{};
    bool mouseClicked_ = false;
    bool closed_ = false;
    bool initialized_ = false;
    int width_ = 0;
    int height_ = 0;
    std::uint32_t lastTickMs_ = 0;
};

} // namespace orbduel
