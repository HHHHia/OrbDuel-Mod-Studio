# AGENTS.md — OrbDuel 工程手册

> 给所有 AI（Claude / Cursor / Copilot / DSH / ChatGPT）和真人队友的唯一规则来源。
> 动手前读完；规则冲突时以本文件为准。改本文件要走 PR，并写明理由。

---

## 0. 30 秒认知

OrbDuel 是一个 **C++17 + SDL2 2.32** 的 2D 双球物理对战游戏。

核心设计一句话：**引擎不认识「球」和「技能」**。引擎只认实体（Body）、修饰器（Modifier）、事件（Event）；
角色、技能、地图全部是 `content/` 下的外部文件夹，运行时可热重载。

这是 CS2310 小组作业（group08，2026-11-24 23:59 截止）。
评分五项 `published / documented / functional / innovative / teamwork` 中，
**innovative 押在「内容模组化」**上：别人拖一个文件夹进来，就能多一个角色、一套技能、一张图。

## 1. 开工前必读（按顺序）

| 文件 | 读它解决什么 |
|---|---|
| `docs/ARCHITECTURE.md` | 分层、数据流、每个模块为什么存在 |
| `docs/CONTENT_FORMAT.md` | 数据格式；**改任何内容前必读** |
| `docs/STATUS.md` | 当前哪些是空壳、哪些能跑、下一步谁做 |
| `docs/MODULE_CONTRACTS/<模块>.md` | 你负责的那块的接口与不变量 |

## 2. 构建 / 运行 / 校验

```powershell
# 配置 + 构建（SDL2 已离线放在 third_party/，不需要联网、不需要 vcpkg）
.\build.ps1

# 只构建不跑
.\build.ps1 -SkipRun

# 冒烟测试：验证工具链、SDL2 链接、内容目录可读
.\build\bin\orbduel.exe --selftest

# 直接开一局（先进主菜单；对局内 ESC 回菜单，退出在菜单里点 QUIT GAME）
.\build\bin\orbduel.exe

# 跳过主菜单直接开局（脚本 / CI）
.\build\bin\orbduel.exe --no-menu

# 无窗口跑 60 秒，回归物理与结算
.\build\bin\orbduel.exe --headless --seconds 60

# 校验所有内容包（schema + 资源引用）
.\tools\validate_content.ps1
```

工具链基准：`g++ 16.2 (w64devkit)` + `CMake` + `Ninja`；MSVC 2022 亦可（CMake 自动选 `third_party/sdl2/msvc`）。

## 3. 铁律（违反就回滚）

1. `include/orbduel/` 是公共契约。改任何**公开函数签名**，必须同步 `docs/MODULE_CONTRACTS/*.md`，并在 commit message 里写 `contract:` 前缀。
2. `core/`、`physics/`、`content/`、`match/` **禁止** `#include <SDL.h>`。只有 `render/` 与 `game/` 可以碰 SDL2。
3. 角色、技能、数值一律走 `content/`，**不许**硬编码进 C++。代码里出现 `"blaze"` 这种角色名就是 bug。
4. 交 commit 前必须两条都绿：构建通过 + `validate_content.ps1` 通过。
5. 不新增第三方依赖（SDL2、单头库除外）。要加先在 `docs/DECISIONS.md` 写一条 ADR。
6. 随机数只能用 `core/Rng.hpp`，禁止 `rand()`、`std::random_device`、`<random>` 的默认种子。
7. 物理只按固定步长 `1/120 s` 推进；渲染帧率**不能**直接影响物理结果。
8. 注释用中文；公开接口写「做什么 / 参数含义 / 不变量」，不写「怎么实现」。
9. 一个 commit 只做一件事。message 用英文，首行 ≤ 60 字符，正文写清原因。
10. 素材必须 CC0 或自制，并在 `content/ASSETS.md` 登记来源 + 许可证。**不要**提交从游戏里抠的图。

## 4. 模块地图

| 目录 | 职责 | 不该做的事 | 契约 |
|---|---|---|---|
| `core/` | Vec2/Rect、固定步长时钟、seeded RNG、事件总线、日志 | 不知道游戏规则 | `MODULE_CONTRACTS/core.md` |
| `physics/` | 圆体、弹性碰撞、场地边界、障碍 | 不知道 HP、技能、贴图 | `MODULE_CONTRACTS/physics.md` |
| `render/` | SDL2 封装、精灵、粒子、调试绘制 | 不推进物理、不改游戏状态 | `MODULE_CONTRACTS/render.md` |
| `content/` | JSON 数据模型、加载、校验、热重载 | 不写死任何具体角色 | `MODULE_CONTRACTS/content.md` |
| `match/` | HP、伤害、连击、缩圈、防僵局、技能冷却 | 不直接画东西 | `MODULE_CONTRACTS/match.md` |
| `game/` | 装配所有模块、主循环、输入 | 不实现具体规则 | `MODULE_CONTRACTS/game.md` |

依赖方向**只能向下**，不能反向：

```
game  →  match  →  physics  →  core
  ↓        ↓          ↓
render ←───┴──────────┘
content ──→ core
```

（`physics` 不依赖 `content`；把 `CharacterDef` 转成 `Body` 是 `match`/`game` 的活。）

## 5. 三个高频任务

### 5.1 加一个角色（不改 C++）

1. 复制 `content/characters/blaze/` 为 `content/characters/<new-id>/`
2. 改 `character.json` 的 `id`、`name`、`stats`，`skills` 填 `content/skills/` 里已有的 id
3. 放一张 `skin.png`（建议 256×256，背景透明）
4. 跑 `.\tools\validate_content.ps1`
5. 启动游戏 → 主菜单里应能看到这个角色（左右两侧都能切到；角色列表读 `content/characters/`，不写死 id。**热重载尚未实现**，改完要重启）

### 5.2 加一个技能

1. 在 `content/skills/` 新建 `<id>.json`，从 `dash.json` 复制改
2. 只允许用 `docs/CONTENT_FORMAT.md` 里列出的 `op` 与 `vfx.type`；**要新原语 = 改引擎**，先写 ADR
3. 把 id 加进某个角色的 `skills` 数组
4. 校验 → 启动 → 触发一次看日志有没有 `skill.cast`

### 5.3 加一张地图

1. 复制 `content/maps/duel-basic/` 为 `content/maps/<new-id>/`
2. 改 `map.json` 的 `size`、`spawns`、`obstacles`、`shrink`
3. 需要背景就放 `bg.png`，尺寸与 `size` 一致
4. 校验 → 启动 → 从地图列表进入

## 6. 常见坑

- **同向同速 = 永不碰撞**。两球无重力、同速平行时能一直不接触。任何改物理的改动都要跑长跑回归（`--headless --seconds 60`；`--autotest` 尚未实现）。
- **改 `Arena` 尺寸必须走 `setShrinkTarget()`**，不要直接改 `bounds`；否则球会卡在墙外。
- **热重载失败不能崩**：`ContentLoader` 加载失败时保留旧包，错误写进屏幕控制台。
- **不要给 `Body` 加游戏字段**（HP 放 `Match` 的 `Fighter` 里）。`physics` 要能被单独复用。
- SDL2 是**动态链接**：`SDL2.dll` / `SDL2_image.dll` / `SDL2_ttf.dll` 必须和 exe 同目录（CMake 会自动拷）。删了 DLL 会直接起不来。
- 提交前确认没有把 `build/`、`.vs/`、`*.pdb` 带进 git。
- **不要换回 raylib / GLFW**：GLFW 初始化手柄时调 `DirectInput8Create`，本机该调用返回 `E_FAIL`，GLFW 直接判定 `glfwInit()` 失败，窗口根本开不出来（ADR-015）。SDL2 的 video 初始化不碰 DirectInput。
- 改 `.ps1` 必须存成 **UTF-8 with BOM**。Windows PowerShell 5.1 会把无 BOM 的 UTF-8 当 GBK 读，中文字符串直接把脚本语法搞崩。

## 7. 提交前自检

- [ ] `.\build.ps1 -SkipRun` 通过
- [ ] `.\tools\validate_content.ps1` 通过
- [ ] 没碰 `include/` 公开签名，或已同步契约文档
- [ ] 这个 commit 只做一件事
- [ ] 新素材已登记到 `content/ASSETS.md`
