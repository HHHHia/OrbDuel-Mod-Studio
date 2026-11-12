#pragma once

// 主菜单：开始游戏 / 退出游戏 / 左右两侧显示与切换对战角色。
//
// 不变量（菜单契约）：
//   1. 只读 ContentPack，不修改任何内容数据
//   2. 不推进时钟与物理：菜单帧不算进对局时间
//   3. 选择结果以 pack 里的下标保存，换内容包后必须重新 open()
//   4. 键盘只响应 Enter（开始）与 ESC（关确认框）；其余交互只走鼠标

#include <string>

#include "orbduel/content/DataModel.hpp"
#include "orbduel/core/Types.hpp"
#include "orbduel/render/AssetCache.hpp"
#include "orbduel/render/Renderer.hpp"

namespace orbduel {

class Menu {
public:
    // 用当前对战角色打开菜单；id 找不到就退回 pack 里第 0 / 第 1 个角色。
    void open(const ContentPack& pack, const std::string& leftId, const std::string& rightId);

    // 处理一帧输入。必须在 renderer.pumpEvents() 之后、draw() 之前调用。
    void update(Renderer& renderer);

    // 画一帧。调用方负责 beginFrame() / endFrame()；chineseLabels 决定名字用中文还是英文。
    void draw(Renderer& renderer, AssetCache& assets, const std::string& contentRoot,
              bool chineseLabels) const;

    // 清屏用的底色，和菜单面板配色配套。
    Rgba background() const;

    bool wantsStart() const;
    bool wantsQuit() const;
    void clearRequests();

    // 当前选中的角色 id；内容为空时返回空串。
    const std::string& leftId() const;
    const std::string& rightId() const;

private:
    const ContentPack* pack_ = nullptr;
    int leftIndex_ = 0;
    int rightIndex_ = 0;
    bool confirmOpen_ = false;
    bool startRequested_ = false;
    bool quitRequested_ = false;
};

} // namespace orbduel
