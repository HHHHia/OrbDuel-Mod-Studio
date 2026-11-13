# 契约：game

- Game::run 装配 Studio；默认打开概览，可切换编辑器、模组工坊和自动对战。
- 新 Options：sceneFile、modsRoot、theme、view、play、hidden；原 seed、角色、地图、秒数和截图参数保留。
- --headless 与 --autotest 在 SDL 初始化前进入 Simulation，不创建窗口。autotest 在指定模拟时间内连续运行多局。
- 编辑器持有草稿、撤销栈与属性面板；开始测试复制文档，返回编辑不写回运行状态。
- SDL 事件先送 ImGui，文字输入时不执行画布快捷键；选择和实际拖动分别处理。
- 保存路径默认 user/arena.json；正常退出保留 last-scene.json，截图运行不写最近草稿。
- 绘制在 SDL_Renderer 后端上调用 Dear ImGui；界面数据来自注册类型，不按具体机关名称分派。
