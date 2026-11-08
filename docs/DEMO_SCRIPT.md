# 1 分钟演示脚本 / Demo Script

> 目标：60 秒内让人记住「**内容是可插拔的**」，而不是"又一个打砖块"。
> 彩排 3 遍，掐表。破 60 秒就砍第 4 段。

## 分镜

| 时间 | 画面 | 你说的话（英文，≤ 2 句） | 备用 |
|---|---|---|---|
| 0:00–0:08 | 游戏窗口已开着，两球对撞中 | "Two orbs, no gravity. Every collision is data-driven." | — |
| 0:08–0:22 | 正常打一局，HP 条掉、火花、连击数字涨 | "Hit damage scales with impact speed, and combos multiply it." | 若没出连击，手动 `--autotest` 展示 |
| 0:22–0:30 | 场地开始缩圈，边界红光 + 球被推回 | "If the match drags, the arena closes in." | 提前把 `shrink.firstAt` 调到 8 s |
| 0:30–0:42 | **切到资源管理器，拖一个 `content/characters/<new>/` 文件夹进去**；切回游戏，2 秒后新角色出现在选择列表 | "This folder is a character — skin, hitbox, stats, skills. No recompile." | 提前准备好文件夹，别现场建 |
| 0:42–0:52 | 改 `skills/dash.json` 的 `value`（比如 220→600），保存；游戏里冲刺距离立刻变长 | "Even the skill numbers are hot-reloaded." | 用记事本，别开 IDE |
| 0:52–1:00 | 切到 README / 仓库页 | "Repo is public, docs are in English and Chinese." | — |

## 演示前检查

- [ ] `--seed 20261124` 启动，保证每次跑同一局（避免现场"怎么不打"）
- [ ] `content/characters/` 下的演示文件夹**提前准备好**，不要现场复制
- [ ] 记事本已打开 `skills/dash.json`，光标停在 `value` 上
- [ ] 窗口 1280×720，字体调到投影能看清
- [ ] 关掉微信/QQ 通知
- [ ] 备用：`--autotest 30` 的日志截图（万一现场卡死）

## 预设 Q&A

**Q: 这和现成的 2D 引擎有什么区别？**
A: 区别不在渲染，在内容边界——引擎里没有任何角色名、技能名；加内容不改代码。

**Q: 为什么不用 Box2D？**
A: 两个圆、无重力、只要弹性碰撞，自研 150 行；而且我们要能随时改场地尺寸（缩圈），Box2D 的静态体要重建。

**Q: 怎么保证一定会碰撞？**
A: 四层：墙反弹抖动、静默吸引、缩圈、场地地板。

**Q: 碰撞箱能自定义吗？**
A: 这版是圆；伤害判定预留着像素掩码接口，运动碰撞仍用圆（避免卡墙）。
