#include "orbduel/render/Renderer.hpp"

#include <SDL_image.h>

#include <algorithm>
#include <cmath>

namespace orbduel {

Renderer::Renderer() = default;

Renderer::~Renderer() { shutdown(); }

bool Renderer::init(int width, int height, const char* title, bool hidden) {
    if (initialized_) {
        return true;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        return false;
    }

    Uint32 flags = SDL_WINDOW_SHOWN;
    if (hidden) {
        flags = SDL_WINDOW_HIDDEN;
    }

    window_ = SDL_CreateWindow(title,
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               width, height, flags);
    if (window_ == nullptr) {
        SDL_Quit();
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, -1,
                                   SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer_ == nullptr) {
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
    }
    if (renderer_ == nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        SDL_Quit();
        return false;
    }

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    width_ = width;
    height_ = height;
    lastTickMs_ = SDL_GetTicks();
    initialized_ = true;
    return true;
}

void Renderer::shutdown() {
    for (auto& entry : fonts_) {
        if (entry.second != nullptr) {
            TTF_CloseFont(entry.second);
        }
    }
    fonts_.clear();
    fontPath_.clear();

    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    if (TTF_WasInit() != 0) {
        TTF_Quit();
    }
    if (initialized_) {
        SDL_Quit();
    }
    initialized_ = false;
}

bool Renderer::isInitialized() const { return initialized_; }

bool Renderer::pumpEvents() {
    pressedKeys_.clear();
    mouseClicked_ = false;
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_QUIT) {
            closed_ = true;
        } else if (event.type == SDL_KEYDOWN) {
            pressedKeys_.push_back(event.key.keysym.scancode);
        } else if (event.type == SDL_MOUSEMOTION) {
            mousePosition_ = Vec2{static_cast<float>(event.motion.x), static_cast<float>(event.motion.y)};
        } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            mousePosition_ = Vec2{static_cast<float>(event.button.x), static_cast<float>(event.button.y)};
            mouseClicked_ = true;
        }
    }
    return !closed_;
}

bool Renderer::wasKeyPressed(SDL_Scancode code) const {
    for (SDL_Scancode pressed : pressedKeys_) {
        if (pressed == code) {
            return true;
        }
    }
    return false;
}

bool Renderer::shouldClose() const { return closed_; }
void Renderer::requestClose() { closed_ = true; }

Vec2 Renderer::mousePosition() const { return mousePosition_; }
bool Renderer::wasMouseClicked() const { return mouseClicked_; }

void Renderer::beginFrame(Rgba clearColor) {
    const SDL_Color color = toSdl(clearColor);
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_RenderClear(renderer_);
}

void Renderer::endFrame() { SDL_RenderPresent(renderer_); }

int Renderer::width() const { return width_; }
int Renderer::height() const { return height_; }

float Renderer::fps() const {
    const std::uint32_t now = SDL_GetTicks();
    const std::uint32_t delta = now - lastTickMs_;
    if (delta == 0) {
        return 0.0f;
    }
    return 1000.0f / static_cast<float>(delta);
}

void Renderer::drawCircle(Vec2 center, float radius, Rgba color) {
    if (radius <= 0.5f) {
        return;
    }
    const SDL_Color c = toSdl(color);
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);

    // 扫描线填充：每行画一条水平线
    const int r = static_cast<int>(radius + 0.5f);
    for (int dy = -r; dy <= r; ++dy) {
        const float halfWidth = std::sqrt(std::max(0.0f, radius * radius - static_cast<float>(dy * dy)));
        if (halfWidth <= 0.0f) {
            continue;
        }
        const float y = center.y + static_cast<float>(dy);
        SDL_RenderDrawLineF(renderer_, center.x - halfWidth, y, center.x + halfWidth, y);
    }
}

void Renderer::drawCircleLines(Vec2 center, float radius, Rgba color) {
    const SDL_Color c = toSdl(color);
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);

    constexpr int kSegments = 48;
    float prevX = center.x + radius;
    float prevY = center.y;
    for (int i = 1; i <= kSegments; ++i) {
        const float angle = (2.0f * kPi) * static_cast<float>(i) / static_cast<float>(kSegments);
        const float x = center.x + std::cos(angle) * radius;
        const float y = center.y + std::sin(angle) * radius;
        SDL_RenderDrawLineF(renderer_, prevX, prevY, x, y);
        prevX = x;
        prevY = y;
    }
}

void Renderer::drawRing(Vec2 center, float radius, float thickness, Rgba color) {
    const float outer = radius + thickness * 0.5f;
    const float inner = std::max(0.0f, radius - thickness * 0.5f);
    drawCircleLines(center, outer, color);
    if (inner > 1.0f) {
        drawCircleLines(center, inner, color);
    }
}

void Renderer::drawLine(Vec2 from, Vec2 to, float thickness, Rgba color) {
    const SDL_Color c = toSdl(color);
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);

    if (thickness <= 1.0f) {
        SDL_RenderDrawLineF(renderer_, from.x, from.y, to.x, to.y);
        return;
    }

    const Vec2 delta = to - from;
    const float len = lengthOf(delta);
    if (len <= 0.0001f) {
        return;
    }
    const Vec2 dir = delta / len;
    const Vec2 normal{-dir.y, dir.x};

    const int layers = static_cast<int>(thickness);
    const float half = (static_cast<float>(layers) - 1.0f) * 0.5f;
    for (int i = 0; i < layers; ++i) {
        const Vec2 offset = normal * (static_cast<float>(i) - half);
        SDL_RenderDrawLineF(renderer_,
                            from.x + offset.x, from.y + offset.y,
                            to.x + offset.x, to.y + offset.y);
    }
}

void Renderer::drawRect(Rect rect, Rgba color) {
    const SDL_Color c = toSdl(color);
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    SDL_FRect r{rect.x, rect.y, rect.w, rect.h};
    SDL_RenderFillRectF(renderer_, &r);
}

void Renderer::drawRectLines(Rect rect, float thickness, Rgba color) {
    const SDL_Color c = toSdl(color);
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    if (thickness <= 1.0f) {
        SDL_FRect r{rect.x, rect.y, rect.w, rect.h};
        SDL_RenderDrawRectF(renderer_, &r);
        return;
    }
    const Vec2 topLeft{rect.x, rect.y};
    const Vec2 topRight{rect.x + rect.w, rect.y};
    const Vec2 bottomRight{rect.x + rect.w, rect.y + rect.h};
    const Vec2 bottomLeft{rect.x, rect.y + rect.h};
    drawLine(topLeft, topRight, thickness, color);
    drawLine(topRight, bottomRight, thickness, color);
    drawLine(bottomRight, bottomLeft, thickness, color);
    drawLine(bottomLeft, topLeft, thickness, color);
}

void Renderer::drawConvexPoly(const Vec2* points, int count, Rgba color) {
    if (renderer_ == nullptr || points == nullptr || count < 3) {
        return;
    }
    // 凸多边形从 0 号顶点扇形三角化，正好 count-2 个三角形
    const SDL_Color c = toSdl(color);
    std::vector<SDL_Vertex> vertices;
    vertices.reserve(static_cast<std::size_t>(count - 2) * 3);
    for (int i = 1; i + 1 < count; ++i) {
        for (const Vec2& p : {points[0], points[i], points[i + 1]}) {
            SDL_Vertex vertex{};
            vertex.position = SDL_FPoint{p.x, p.y};
            vertex.color = c;
            vertices.push_back(vertex);
        }
    }
    SDL_RenderGeometry(renderer_, nullptr, vertices.data(), static_cast<int>(vertices.size()),
                       nullptr, 0);
}

void Renderer::drawSprite(SDL_Texture* texture, Vec2 center, float rotationDeg, float scale, Rgba tint) {
    if (texture == nullptr || scale <= 0.0f) {
        return;
    }

    int texWidth = 0;
    int texHeight = 0;
    SDL_QueryTexture(texture, nullptr, nullptr, &texWidth, &texHeight);
    if (texWidth <= 0 || texHeight <= 0) {
        return;
    }

    const float w = static_cast<float>(texWidth) * scale;
    const float h = static_cast<float>(texHeight) * scale;
    SDL_FRect dst{center.x - w * 0.5f, center.y - h * 0.5f, w, h};

    SDL_SetTextureColorMod(texture, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(texture, tint.a);
    SDL_RenderCopyExF(renderer_, texture, nullptr, &dst, rotationDeg, nullptr, SDL_FLIP_NONE);
    SDL_SetTextureColorMod(texture, 255, 255, 255);
    SDL_SetTextureAlphaMod(texture, 255);
}

bool Renderer::loadFont(const std::string& ttfPath, int baseSize) {
    if (TTF_WasInit() == 0) {
        if (TTF_Init() != 0) {
            return false;
        }
    }
    for (auto& entry : fonts_) {
        if (entry.second != nullptr) {
            TTF_CloseFont(entry.second);
        }
    }
    fonts_.clear();

    fontPath_ = ttfPath;
    baseFontSize_ = baseSize;

    TTF_Font* font = TTF_OpenFont(ttfPath.c_str(), baseSize);
    if (font == nullptr) {
        return false;
    }
    fonts_[baseSize] = font;
    return true;
}

TTF_Font* Renderer::fontFor(int fontSize) const {
    const int size = fontSize > 0 ? fontSize : baseFontSize_;
    auto it = fonts_.find(size);
    if (it != fonts_.end()) {
        return it->second;
    }
    if (fontPath_.empty()) {
        return nullptr;
    }
    TTF_Font* font = TTF_OpenFont(fontPath_.c_str(), size);
    if (font == nullptr) {
        return nullptr;
    }
    fonts_[size] = font;
    return font;
}

void Renderer::drawText(const std::string& text, Vec2 position, int fontSize, Rgba color) {
    if (text.empty()) {
        return;
    }
    TTF_Font* font = fontFor(fontSize);
    if (font == nullptr) {
        return;
    }

    const SDL_Color c = toSdl(color);
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), c);
    if (surface == nullptr) {
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    if (texture != nullptr) {
        SDL_FRect dst{position.x, position.y,
                      static_cast<float>(surface->w), static_cast<float>(surface->h)};
        SDL_RenderCopyF(renderer_, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

int Renderer::measureText(const std::string& text, int fontSize) const {
    if (text.empty()) {
        return 0;
    }
    TTF_Font* font = fontFor(fontSize);
    if (font == nullptr) {
        return 0;
    }
    int w = 0;
    int h = 0;
    TTF_SizeUTF8(font, text.c_str(), &w, &h);
    return w;
}

SDL_Renderer* Renderer::backend() const { return renderer_; }

bool Renderer::saveScreenshot(const std::string& path) {
    if (renderer_ == nullptr || width_ <= 0 || height_ <= 0) {
        return false;
    }

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width_, height_, 32,
                                                          SDL_PIXELFORMAT_ARGB8888);
    if (surface == nullptr) {
        return false;
    }

    const bool readOk = SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_ARGB8888,
                                             surface->pixels, surface->pitch) == 0;
    const bool saved = readOk && IMG_SavePNG(surface, path.c_str()) == 0;
    SDL_FreeSurface(surface);
    return saved;
}

} // namespace orbduel
