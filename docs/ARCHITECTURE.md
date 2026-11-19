# 当前架构 / Architecture

## 分层与数据流

```text
C++ 模组 DLL ──注册类型/参数/回调──> PluginManager
                                         │
ContentLoader ──角色/技能──> SceneDocument ←┴─ 编辑器 / 种子生成 / 文件与分享码
                                  │ 开始测试时复制快照
                                  ▼
                              Simulation
                         ┌────────┼────────┐
                      Match     World   模组实例
                         └────────┼────────┘
                            事件与状态
                                  ▼
                    SDL2 / Dear ImGui / Themes
```

`core` 定义数学、随机和事件；`physics` 只处理物理；`content` 加载基础内容和地图文档；`match` 管理比赛与固定步；`mods` 发现独立插件并保存注册表；`game` 装配与编辑器操作；`render` 负责背景和 SDL 资源。`Simulation` 使用模组注册表，但插件管理器不依赖具体比赛实现。

## 地图与运行状态

编辑器和种子生成器操作 `SceneDocument`；保存与分享都序列化同一文档。点击测试创建独立 `Simulation`，其中拥有地图、角色和技能快照。实体移动、HP、机关销毁等只改变运行快照，返回编辑器不复制回草稿。

每步：自定义事件 → 模组持续行为 → 比赛计时与内置技能 → 场地收缩与退化终局检查 → 物理积分与碰撞 → 挤压伤害 → 区域 enter/stay/leave → 派发事件 → 判胜负 → 对象增删。绘制只读取状态。所有物理与行为步长均为 1/120 秒；装饰动画不消耗比赛随机数。

整合后，缩圈边界更新和退化场地终局检查位于物理积分之前；无法容纳角色时按原始 HP 结算并跳过物理。战斗层同时管理通用修饰器、蓄力学习、被动贯穿、引力成长和万向天引，所有伤害经过统一带来源的入口。

## 原生扩展

公开 SDK 在 `mod-sdk/include/orbduel_mod.hpp`。独立 DLL 导出 `orbduel_register`，注册类型描述和实例回调。C++ 便捷包装隐藏函数表细节，作者写 step/event/draw 即可新增机制。宿主不按机关名称分派逻辑，示例类型均来自 DLL。

回调里的机关增删在步末提交；实例销毁后再卸载 DLL。重新加载仅发生在菜单/编辑器。模组使用与宿主相同的操作系统、CPU 架构和工具链构建。

## 分享与风格

分享码为 `OD1:` + 地图 JSON 的 Base64URL 文本，包含实际布局、参数、规则、角色配置和风格。模组按文本 ID 查找，代码/素材另行分发。不增加哈希、签名、版本锁或依赖解析。

五套风格由 C++ 程序化绘制；参数包括密度、动画、背景与强调色。编辑器使用 Dear ImGui 的官方 SDL2/SDL_Renderer2 后端，保持原有 SDL2 运行环境。

## 早期设计变更

ADR-016 将扩展机制从纯 JSON 固定效果扩展为独立 C++ DLL。内容与 DLL 使用菜单显式重新加载，删除原先没有实现的自动 poll 接口。新增接口、文件格式、验收结果以 MOD_STUDIO、CONTENT_FORMAT 和 STATUS 为准。

## 战斗实体扩展

World 使用稳定 ID 与可回收存储，Match 持有动态 Fighter/SkillSystem 和额外角色定义快照；Simulation 在步首/步末统一提交实体与能力增删。原 SDK 布局不变，独立 EntityApi 通过可选 DLL 入口注入。实体仍以 Body 参与物理，HP 留在 Match；召唤与 AI 逻辑由模组实现。详细生命周期和示例见 [战斗实体与分身模组](COMBAT_ENTITIES.md)。

## Windows / macOS 源码适配

核心和玩法继续为 C++17。CMake 根据平台选择 MSVC 离线 SDL 库或 Apple Clang/pkg-config，PluginManager 使用原生动态库函数且保持原 SDK 布局；Mac 的用户数据和自制模组独立于游戏源目录。Shell/CMake 只负责构建安装。新增 Mac 入口作为未编译、未实机验证的适配源码交付；Windows 回归与独立工程验证见 VALIDATION，Mac 上手见 MACOS。

C++ 作者层 orbduel_abilities.hpp 仅组合现有 Host/EntityApi；状态与算法归模组 DLL，宿主维持通用生命周期。回溯和护卫是独立注册类型，内部控制器不进入玩家放置和随机地图生成列表。
