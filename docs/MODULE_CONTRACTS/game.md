# 契约：game

- 工作台取代旧菜单实现（独立 `Menu` 类已删除）；--no-menu 是 --play 的兼容入口，--lang en|zh 控制角色/技能名称，--shot-at 指定截图时刻。
- 显式 --map 不生成额外机关，使用原地图布局、缩圈与无时限规则；地图文件仍使用自身的快照规则。
- 角色面板可编辑 ghost/contact/massMax/massRamp；三角形、蓄力方向与引力范围在画布中显示，HUD 区分被动/蓄力/牵引/冷却。

- runStudio()（game/Studio.hpp）是进程入口，负责装配内容、模组与工作台；默认打开概览，可切换编辑器、模组工坊和自动对战。
- 新 Options：sceneFile、modsRoot、theme、view、play、hidden；原 seed、角色、地图、秒数和截图参数保留。
- --headless 与 --autotest 在 SDL 初始化前进入 Simulation，不创建窗口。autotest 在指定模拟时间内连续运行多局。
- 编辑器持有草稿、撤销栈与属性面板；开始测试复制文档，返回编辑不写回运行状态。
- SDL 事件先送 ImGui，文字输入时不执行画布快捷键；选择和实际拖动分别处理。
- 保存路径默认 user/arena.json；正常退出保留 last-scene.json，截图运行不写最近草稿。
- 绘制在 SDL_Renderer 后端上调用 Dear ImGui；界面数据来自注册类型，不按具体机关名称分派。
