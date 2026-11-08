#pragma once

// 调试层：F1 碰撞箱 / F2 场地边界 / F3 事件流 / F4 帧信息。
// 只在 Debug 构建或 --debug 下由 game 调用。

#include <string>
#include <vector>

#include "orbduel/core/Types.hpp"

namespace orbduel {

class Renderer;
class World;
class Arena;

enum class DebugLayer {
    Hitboxes = 0,
    Arena,
    Events,
    Count
};

class DebugDraw {
public:
    DebugDraw();
    ~DebugDraw() = default;

    void toggle(DebugLayer layer);
    bool enabled(DebugLayer layer) const;

    void draw(Renderer& renderer, const World& world, const Arena& arena);

    // 屏幕左下角滚动的事件行（最多保留 12 行）。
    void pushEvent(const std::string& line);
    void clear();

private:
    static constexpr int kLayerCount = static_cast<int>(DebugLayer::Count);
    bool enabled_[kLayerCount] = {false, false, false};
    std::vector<std::string> eventLines_;
};

} // namespace orbduel
