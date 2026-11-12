# OrbDuel

Two orbs, one arena, zero gravity. Character / skill / map are **data folders** you can hot-reload.

两个球、一块场地、没有重力。角色 / 技能 / 地图都是**外部文件夹**，改完不用重编译。

---

## 这是什么 / What this is

一个 C++17 + SDL2 的 2D 双球物理对战游戏，重点不在玩法多复杂，而在**内容与引擎解耦**：
引擎只提供「实体 + 修饰器 + 事件」，角色和技能全部由 `content/` 下的 JSON 描述。
往 `content/characters/` 里丢一个文件夹，游戏里就多一个角色。

A 2D two-orb physics duel in C++17 + SDL2. The point is not the gameplay depth — it is
**content/engine separation**: the engine only knows bodies, modifiers and events; every
character and skill is described by JSON under `content/`. Drop a folder into
`content/characters/` and a new character appears.

## 快速开始 / Quick start

需要 / Requires: CMake ≥ 3.20，Ninja，MinGW-w64 g++ 13+（或 MSVC 2022）。
SDL2 2.32.10 已放在 `third_party/`，**不需要联网、不需要 vcpkg**。

```powershell
.\build.ps1                     # 配置 + 构建 + 跑冒烟测试
.\build\bin\orbduel.exe         # 启动游戏
.\tools\validate_content.ps1    # 校验所有内容包
```

手动构建 / Manual build:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## 一局是什么样 / A round

启动先进主菜单：左右两张卡各自用 `<` `>` 切换角色（可以选同一个角色互打），
`START GAME` 或 Enter 开局，`QUIT GAME` 弹确认框（点 No / 点框外 / ESC 取消）。
对局内 `ESC` 回主菜单；结算后显示 `RETURNING IN 5...`，期间按 `R` 重开同阵容，5 秒后自动回菜单。

两球自动对撞（当前没有玩家输入）：碰撞本身不掉血（`damageScale = 0`），伤害只来自技能与缩圈；
Lancer（疾锋）是个橙色瘦长三角，尖角永远朝速度方向；它的 Dash 会原地蓄力 3 秒（越放越短）、沿一条会抖的虚线冲出去撞人，速度够快时撞上对手不弹开、直接穿过去（贯穿被动）；
Singularity（黑洞，`hitbox.ghost`）能让对手穿进身体：被动「吸引」把范围内的对手拖成螺旋轨道，进了核心就掉血并被随机传送走；大招「万向天引」范围吸人、2 秒后原地爆炸，期间自己无敌、被动失效，越放范围越大、冷却越短（10 s → 5 s）；
Wooden Dummy（木人桩）是磐石留下的陪练，1000 血、没有技能；
25 秒后开始缩圈，每 10 秒一档一路缩到 0，场地装不下两个球的那一步就按 HP 高者结算；5 秒没有碰撞会触发极弱的「静默吸引」。
角色名与技能名默认显示英文；加 `--lang zh` 且系统有中文字体时显示中文。

![screenshot](docs/screenshot.png)

| 参数                           | 作用                       |
| ------------------------------ | -------------------------- |
| `--char <id>` / `--enemy <id>` | 菜单里的预选角色（默认前两个） |
| `--map <id>`                   | 指定地图                   |
| `--seed <n>`                   | 固定随机种子，同一局可复现 |
| `--seconds <n>`                | 跑 n 秒后自动退出          |
| `--screenshot <file>`          | 第 2 秒存一张 PNG 后退出（当时在菜单就截菜单） |
| `--shot-at <sec>`              | 配合 `--screenshot`：改成第 N 秒截图（默认 2） |
| `--headless`                   | 不开窗口，只跑逻辑（自动跳过菜单） |
| `--no-menu`                    | 跳过主菜单，直接开局（脚本用） |
| `--lang <en\|zh>`              | 名字语言，默认 `en`；`zh` 需要系统中文字体 |

## 内容长什么样 / What content looks like

```
content/characters/lancer/
  character.json      # 皮肤、碰撞半径、质量、HP、技能列表
  skin.png            # 贴图（建议 256×256，透明背景）
```

```json
{
  "format": 1,
  "id": "lancer",
  "name": { "en": "Lancer", "zh": "疾锋" },
  "skin":   { "texture": "skin.png", "scale": 1.0 },
  "hitbox": { "shape": "circle", "radius": 24.0 },
  "stats":  { "mass": 1.0, "restitution": 0.95, "maxHp": 100.0, "startSpeed": 420.0 },
  "skills": ["dash", "pierce"]
}
```

完整字段见 [`docs/CONTENT_FORMAT.md`](docs/CONTENT_FORMAT.md)。

## 文档 / Docs

| 文档                                               | 内容                             |
| -------------------------------------------------- | -------------------------------- |
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)     | 分层、数据流、防僵局与缩圈设计   |
| [`docs/CONTENT_FORMAT.md`](docs/CONTENT_FORMAT.md) | 角色 / 技能 / 地图 JSON 规范     |
| [`docs/DECISIONS.md`](docs/DECISIONS.md)           | 关键技术决策与理由（ADR）        |
| [`docs/ROADMAP.md`](docs/ROADMAP.md)               | 15 天计划、分工、DoD、砍功能清单 |
| [`docs/DEMO_SCRIPT.md`](docs/DEMO_SCRIPT.md)       | 1 分钟演示分镜                   |
| [`docs/STATUS.md`](docs/STATUS.md)                 | 当前实现状态（AI 靠它找落点）    |
| [`AGENTS.md`](AGENTS.md)                           | 给 AI 和队友的工程规则           |

## 技术栈 / Stack

- C++17，CMake + Ninja
- SDL2 2.32.10 + SDL2_image + SDL2_ttf（动态链接，MinGW / MSVC 两套预编译包，DLL 自动拷贝）
- 自研圆-圆物理（无重力、固定步长 120 Hz、seeded 随机）

## 团队 / Team

CS2310 group08 — 6 人。模块归属见 `docs/ROADMAP.md`。

## 许可证 / License

代码 MIT。第三方：SDL2 / SDL2_image / SDL2_ttf（zlib），见 `third_party/sdl2/*/LICENSE-*.txt`。
游戏素材逐个登记在 `content/ASSETS.md`，**只收 CC0 或自制**。
