#pragma once

// 可执行入口的装配层。
//
// 不变量（game 契约）：
//   1. 物理步数只由 Clock 决定
//   2. 场景切换不重建 ContentLoader（热重载跨场景保持）
//   3. --autotest 不开窗口，输出一行机器可读汇总

#include <cstdint>
#include <string>

namespace orbduel {

struct Options {
    std::string contentRoot = "content";
    std::string mapId;          // 空 = 用第一个地图
    std::string characterId;    // 空 = 用第一个角色
    std::string enemyId;        // 空 = 用第二个角色（不足则复用第一个）
    std::uint32_t seed = 0;     // 0 = 用地图 seed，地图没有则用时间戳
    bool selftest = false;      // 只验证环境，不玩游戏
    bool list = false;          // 只列内容包
    bool headless = false;      // 不创建窗口
    bool debug = false;         // 打开调试层
    float autotestSeconds = 0.0f; // >0 时无窗口跑物理回归
    std::string sceneFile, modsRoot, theme, view;
    bool play = false;
    bool hidden = false;
    float runSeconds = 0.0f;    // >0 时跑够这么多秒自动退出（自测用）
    std::string screenshotPath; // 非空时在第 2 秒存一张 PNG 然后退出
};

class Game {
public:
    Game();
    ~Game();

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    // 返回进程退出码：0 正常 / 2 初始化失败 / 3 内容致命错误。
    int run(const Options& options);
};

} // namespace orbduel
