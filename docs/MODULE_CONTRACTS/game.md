# 契约：game

- 工作台取代旧菜单实现（独立 `Menu` 类已删除）；--no-menu 是 --play 的兼容入口，--lang en|zh 控制界面、角色和技能语言，--shot-at 指定截图时刻。
- 工作台界面皮肤从 `content/ui/skins/<id>.json` 读取；`--ui-skin <id>` 可覆盖保存在 `user/settings.json` 的选择，配置无效时保留当前皮肤。皮肤只控制界面，不改变地图风格或分享码。
- 退出入口在左侧导航底部；点击弹“退出 / 取消”确认，窗口 X 走同一个确认。首页 ESC 不触发退出。
- 显式 --map 不生成额外机关，使用原地图布局、缩圈与无时限规则；地图文件仍使用自身的快照规则。
- 角色面板可编辑 ghost/contact/massMax/massRamp；三角形、蓄力方向与引力范围在画布中显示，HUD 区分被动/蓄力/牵引/冷却。

- runStudio()（game/Studio.hpp）是进程入口，负责装配内容、模组与工作台；默认打开概览，可切换编辑器、模组工坊和自动对战。
- 新 Options：sceneFile、modsRoot、theme、view、play、hidden；原 seed、角色、地图、秒数和截图参数保留。
- --headless 与 --autotest 在 SDL 初始化前进入 Simulation，不创建窗口。autotest 在指定模拟时间内连续运行多局。
- 编辑器持有草稿、撤销栈与属性面板；开始测试复制文档，返回编辑不写回运行状态。
- SDL 事件先送 ImGui，文字输入时不执行画布快捷键；选择和实际拖动分别处理。
- 音效走 `src/game/Audio.cpp`（SDL2 原生音频，不引入 SDL_mixer），素材是 `content/audio/orbduel/*.wav`：挂事件总线上的 Hit / WallBounce / Death / RoundEnd，外加主要按钮的点击声。设备打不开或素材缺失时静音继续，不影响对局；`--sfx-test` 依次试听。
- 保存路径默认 user/arena.json；正常退出保留 last-scene.json，截图运行不写最近草稿。
- 绘制在 SDL_Renderer 后端上调用 Dear ImGui；界面数据来自注册类型，不按具体机关名称分派。

- 游戏初始化/重新加载时注入可选实体函数表。画布枚举运行中的所有 Fighter，使用其角色快照绘制皮肤/特殊技能，额外实体显示小血条；HUD 与终局仍显示两名主角色。编辑器预览不推进召唤。

- macOS 通过 SDL_GetPrefPath("OrbDuel", "OrbDuel") 定位设置、草稿与额外模组目录，并支持 Command 作为编辑快捷键。Windows 使用项目旁的 user/。内容解析支持 SDL 的应用资源路径，界面沿用随包的 Noto Sans SC 字体、语言设置和数据皮肤。
- 放置列表、工坊可用能力和能力计数隐藏 placement < 0 的内部类型。运行时可按 ID 查找并绑定这些类型。编辑器中的自建能力使用“附着”选择左右角色。
