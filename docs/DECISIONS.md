# 决策记录 / Decisions

> 每条都是「背景 → 决定 → 代价」。AI 最容易犯的错是「顺手优化掉」这些决定，
> 所以想推翻任何一条，先在这里加一条新的 ADR，写清为什么，再动代码。
>
> 编号固定、不复用：ADR-014（脚手架不初始化 git）在仓库上线后失效，已删除，编号留空。

---

## ADR-001 渲染用 SDL2 2.32

**背景**：允许第三方库；机器上只有 MinGW（无 MSVC），队友环境未知；要 15 天内出可演示的东西。
最初选的是 raylib，后来因为 ADR-015 的 DirectInput 事故整体迁移到 SDL2。

**决定**：SDL2 2.32.10 + SDL2_image 2.8.12 + SDL2_ttf 2.24.0，动态链接，
MinGW 与 MSVC 两套预编译包放进 `third_party/sdl2/`，DLL 由 CMake 拷到 exe 旁边。

**代价**：
- SDL2 没有内置字体、没有画圆/圆环的 API，`render/` 要多写一层胶水（字体文件、三角扇近似圆）；
- 动态链接意味着**必须带 DLL**，漏拷就起不来（CMake 已自动处理）；
- 换来了「不依赖 GLFW/OpenGL 上下文初始化」这一条关键稳定性。

---

## ADR-002 render 层直接依赖 SDL2，不做后端抽象

**背景**：一开始想加 `IRenderer` 虚接口，方便"以后换后端"。

**决定**：不做。`render/` 里直接调 SDL2，其他模块不 include `<SDL.h>`。

**理由**：换后端这件事在 15 天内不会发生（而且刚换过一次，不想再换）；
虚接口会让三个队友都得读懂 PIMPL 才能画一个圆。真到要换的那天，`render/` 目录是唯一改动点。

**代价**：`Renderer` 的公开签名里出现 `SDL_Texture*`、`SDL_Renderer*` 等 SDL 类型。

---

## ADR-003 自研圆-圆物理，固定步长 120 Hz

**背景**：Box2D 成熟，但引入第二个依赖、且它的碰撞箱是刚体形状，和"按皮肤重画碰撞箱"的诉求打架。

**决定**：自研，只做圆 + 矩形/圆形障碍，解析解，固定步长 `1/120`。

**代价**：
- 没有旋转、没有摩擦、没有连续碰撞检测（CCD）——本项目也不需要；
- 高速度下可能穿模，用 `maxSpeed = 1800 px/s` 兜底。

**不变量**：物理结果只由 `seed + 固定步数` 决定，和帧率无关。

---

## ADR-004 事件在步末统一派发（队列，不是即时回调）

**背景**：物理碰撞回调里如果直接结算伤害、再触发特效，容易出现「回调里再 publish」的重入。

**决定**：`EventBus::publish` 只入队；`dispatch()` 在每个固定步末尾统一分发。

**代价**：事件有一帧（1/120 s）延迟，肉眼不可见。

---

## ADR-005 内容包 + 热重载 = 创新点

**背景**：评分项要求 "reusable feature not easily replaceable by existing packages"。
物理手感和玩法深度都撑不起这个描述；能撑起来的是"内容即文件夹"。

**决定**：角色/技能/地图全部外部 JSON，运行时轮询 `last_write_time` 热重载；校验失败不崩。

**代价**：
- 多一层数据模型（`DataModel.hpp` 约 12 个结构体）；
- 所有数值都要从 JSON 走，写死一个数字就破坏了这个故事。

**演示动作**：现场往 `content/characters/` 拖一个文件夹 → 2 秒内新角色出现。

---

## ADR-006 核心类型自己定义，不暴露 SDL 类型给 physics/content

**背景**：SDL2 提供 `SDL_Point` / `SDL_FPoint` / `SDL_Color` / `SDL_Rect`，直接用最省事。

**决定**：`core/Types.hpp` 定义 `Vec2` / `Rgba` / `Rect`，只在 `render/` 边界用
`toSdl()` 转成 SDL 类型。

**理由**：`physics` 和 `content` 要能脱离渲染后端单独测试；`Body` 不该背 `<SDL.h>`。

**代价**：边界处有几行 `SDL_FPoint{v.x, v.y}` 的样板。

---

## ADR-007 单一 seeded RNG（xorshift64*），禁止 rand()

**背景**：缩圈、初速抖动、碰撞扰动都需要随机；但 demo 要可复现。

**决定**：全局只允许一个 `Rng` 实例，由 `map.seed` 或 `--seed` 初始化；所有随机都从它取。

**代价**：多人同时写随机逻辑时要传引用，不能各建各的（会破坏复现）。

---

## ADR-008 事件用带 tag 的结构体，不用模板 + std::variant

**背景**：`template<class E> subscribe()` 最好用，但实现必须写在头文件里，队友读起来也绕。

**决定**：`struct Event { EventType type; int a; int b; Vec2 pos; float value; }`，字段含义由 `type` 决定。

**代价**：加新事件要同时改枚举和文档；类型安全靠人，不靠编译器。
（如果以后事件超过 12 种，再考虑 `std::variant`。）

---

## ADR-009 防僵局做四层，缺一不可

**背景**：无重力双球在矩形场地里可以长期不碰撞（见 ARCHITECTURE §6）。

**决定**：L1 墙反弹抖动、L2 静默吸引、L3 缩圈、L4 场地地板。

**代价**：
- 玩家可能感觉"球会自己靠近"，所以 L2 必须缓慢渐入、且只在静默期生效；
- 缩圈必须做得**像设计**（预警 + 动画 + 音效），不能像 bug。

---

## ADR-010 缩圈压到球 = 持续少量伤害

**背景**：只推回的话，缩圈对局势没有压力；直接判负又太狠。

**决定**：`damagePerSec = 4 HP/s`，且**不计入连击**。

**代价**：需要给"环境伤害"单独一条结算路径，不能复用命中伤害（否则连击倍率会被刷）。

---

## ADR-011 单局 HP 制，但规则走接口

**背景**：先做单局 HP 归零；以后可能加限时、三局两胜。

**决定**：`IMatchRule` 接口 + `HpRule` 默认实现；`Match` 只依赖接口。

**代价**：多一层间接；换来加模式时不改 `Match`。

---

## ADR-012 文档中英双语，代码注释中文

**背景**：作业要求英文 README（助教看），队友是中文母语；原文没规定注释语言。

**决定**：`README.md` 双语；`docs/` 中文为主、标题与关键术语带英文；代码注释中文。

**代价**：文档维护量翻倍，所以只对 README 做双语，`docs/` 不逐句翻译。

---

## ADR-013 依赖离线预编译，仓库自带 SDL2

**背景**：队友环境未知（可能是 VS Code + MinGW，也可能是 Visual Studio）。

**决定**：`third_party/sdl2/{mingw,msvc}` 两套包进仓库（约 16 MB）；`CMakeLists.txt` 按编译器自动选。

**代价**：仓库变大；换来"clone 下来就能编"。

---

## ADR-015 从 raylib 迁到 SDL2（事故记录）

**背景**：第一版用 raylib 6.2 预编译包。冒烟程序能编能链，但一开窗就：

```
WARNING: GLFW: Error: 65544 Description: Win32: Failed to create interface
FATAL: PLATFORM: Failed to initialize graphics device
```

**排查**（在本机和普通终端都复现）：
1. 报错字符串来自 GLFW 3.4 `win32_joystick.c`：`DirectInput8Create` 失败时打印；
2. 单独调 `DirectInput8Create` → 返回 `0x80004005 (E_FAIL)`，与 COM 初始化无关；
3. 同样的进程里，原生 Win32 窗口创建成功、OpenGL 4.6（RTX 5060）上下文也创建成功；
4. GLFW 的 `_glfwInitWin32` 把 `_glfwInitJoysticksWin32()` 的返回值当致命错误 →
   `glfwInit()` 直接失败 → raylib `InitWindow` fatal。

结论：**本机 DirectInput 不可用**，而 raylib 没有开关能关掉 GLFW 的手柄初始化。

**决定**：换 SDL2。SDL2 的 `SDL_Init(SDL_INIT_VIDEO)` 不初始化手柄子系统，不碰 DirectInput。
探针验证：`window OK (hidden 320x200, video=windows, renderer=direct3d)`。

**代价**：
- `render/` 两个头文件重写（当时实现还没写，成本很低）；
- 失去 raylib 内置字体与画圆，`render/` 要多写一层胶水；
- 仓库里多 16 MB 预编译包。

**教训**：选渲染库之前，先跑一个 30 行的「开窗 + 清屏」探针，别等 UI 写完了才发现初始化就挂。
