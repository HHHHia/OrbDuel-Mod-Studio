#pragma once

// 进程入口参数：命令行解析结果与工作台运行参数。
//
// 字段语义由 game 层解释（runStudio）；core / physics / content / match 不感知它。

#include <cstdint>
#include <string>

namespace orbduel {

struct Options {
    std::string contentRoot = "content";
    std::string mapId;            // 空 = 用第一个地图
    std::string characterId;      // 空 = 用第一个角色
    std::string enemyId;          // 空 = 用第二个角色（不足则复用第一个）
    std::uint32_t seed = 0;       // 0 = 用地图 seed，地图没有则用时间戳
    bool selftest = false;        // 只验证环境，不玩游戏
    bool list = false;            // 只列内容包
    bool headless = false;        // 不创建窗口
    bool startInMenu = true;      // false = 跳过主菜单直接开局（--no-menu）
    bool debug = false;           // 打开调试层
    float autotestSeconds = 0.0f; // >0 时无窗口跑物理回归
    std::string sceneFile, modsRoot, theme, view;
    bool play = false;
    bool hidden = false;
    float runSeconds = 0.0f;     // >0 时跑够这么多秒自动退出（自测用）
    std::string screenshotPath;  // 非空时在 shotAtSeconds 存一张 PNG 然后退出
    float shotAtSeconds = 2.0f;  // 截图时刻（秒）；调试/文档用，不影响游戏逻辑
    std::string language = "en"; // 界面语言：en = 英文名（默认）/ zh = 中文名（需要系统中文字体）
};

} // namespace orbduel
