# 1 分钟演示脚本 / Demo Script

> 目标：60 秒内让人记住「**内容是可插拔的**」，而不是"又一个打砖块"。
> 彩排 3 遍，掐表。破 60 秒就砍第 4 段。

## 分镜

| 时间 | 画面 | 你说的话（英文，≤ 2 句） | 备用 |
|---|---|---|---|
| 0:00–0:08 | 主菜单点 `START GAME`，两球对撞中 | "Two orbs, no gravity. Every character and skill here is just a folder." | — |
| 0:08–0:24 | 疾锋原地蓄力、沿虚线冲出去撞人，对手 HP 掉；黑洞把疾锋拖成螺旋，场上每秒掉血 | "Charge time, pull radius, damage per second — all of it comes from JSON." | 没出蓄力就讲 `--headless --seconds 30` 的日志（`--autotest` 未实现） |
| 0:24–0:36 | 万向天引：吸力圈收拢 → 原地爆炸 | "The ult is a data-defined pull field plus a blast — and it is drawn by the FX layer, not the skill." | 提前录好 `--screenshot ult.png --shot-at 12` 的那一张 |
| 0:36–0:48 | **切到资源管理器，把 `content/characters/<new>/` 拖进去**；重启 exe，回主菜单，新角色出现在左右可切换列表 | "This folder is a character — skin, hitbox, stats, skills. No recompile." | 热重载未接：要重启 exe 才看得到；文件夹提前准备好 |
| 0:48–0:54 | 改 `skills/gravity-well.json` 的 `range`（比如 280→600），保存重启，吸力圈明显变大 | "Even the pull radius is data." | 热重载未接，别承诺 "hot-reloaded" |
| 0:54–1:00 | 切到 README / 仓库页 | "Repo is public, docs are in English and Chinese." | — |

## 演示前检查

- [ ] `--seed 20261124` 启动，保证每次跑同一局（避免现场"怎么不打"）
- [ ] `content/characters/` 下的演示文件夹**提前准备好**，不要现场复制
- [ ] 记事本已打开 `skills/gravity-well.json`，光标停在 `range` 上
- [ ] 窗口 1280×720，字体调到投影能看清
- [ ] 关掉微信/QQ 通知
- [ ] 备用：`--headless --seconds 30` 的日志截图（万一现场卡死；`--autotest` 未实现）

## 预设 Q&A

**Q: 这和现成的 2D 引擎有什么区别？**
A: 区别不在渲染，在内容边界——引擎里没有任何角色名、技能名；加内容不改代码。

**Q: 为什么碰撞不掉血？**
A: 那是地图数据（`duel-basic` 的 `damageScale = 0`）；想让碰撞掉血就把这个数字调上去，代码一行不改。

**Q: 为什么不用 Box2D？**
A: 两个圆、无重力、只要弹性碰撞，自研 150 行；而且我们要能随时改场地尺寸（缩圈），Box2D 的静态体要重建。

**Q: 怎么保证一定会碰撞？**
A: 四层：墙反弹抖动、静默吸引、缩圈、场地地板。

**Q: 碰撞箱能自定义吗？**
A: 这版运动碰撞仍是圆；伤害判定预留着像素掩码接口。黑洞用的是 `hitbox.ghost`——对手能穿进身体，但碰撞照发。
