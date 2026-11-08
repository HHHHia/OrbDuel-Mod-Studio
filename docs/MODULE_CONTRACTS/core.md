# 契约：core

**Owner**：A　**依赖**：无（只依赖标准库）　**被依赖**：全部

## 职责

- 基础数学类型：`Vec2` / `Rgba` / `Rect`
- 固定步长时钟 `Clock`
- 可复现随机数 `Rng`
- 队列式事件总线 `EventBus`
- 日志 `log::*`

## 不负责

- 不知道"球""技能""HP"是什么
- 不 include SDL
- 不做文件 IO（那是 `content`）

## 公开接口

| 文件 | 内容 |
|---|---|
| `core/Types.hpp` | `Vec2`（含 + - * /、dot、length、normalize）、`Rgba`、`Rect`、`degToRad`、`clampf` |
| `core/Time.hpp` | `Clock::tick(double)`、`Clock::consumeSteps()`、`Clock::alpha()`、`Clock::fixedDt()` |
| `core/Rng.hpp` | `Rng(seed)`、`nextU32()`、`nextFloat()`、`range(lo,hi)`、`unitVector()` |
| `core/Event.hpp` | `EventType`、`Event`、`EventBus::subscribe/publish/dispatch` |
| `core/Log.hpp` | `log::info/warn/error`、`log::recentLines()` |

## 不变量

1. `Clock` 每帧最多消费 `maxSteps` 个固定步；超出部分**丢弃**，不许无限补步。
2. `EventBus::publish` 只入队；只有在 `dispatch()` 里才调用订阅者。
3. `dispatch()` 期间新 publish 的事件留到下一次 dispatch（不递归）。
4. `Rng` 同一 seed 同一序列；`nextFloat()` 返回 `[0,1)`。
5. 所有函数对空/零向量有定义行为（`normalize` 返回 `{0,0}`）。

## 禁止

- `rand()`、`std::random_device`、`<random>` 默认引擎
- 在 core 里 `#include <SDL.h>`
- 在事件订阅回调里 `publish`

## 验收

- `--autotest` 能复现同一 seed 的碰撞计数（连续跑两次数字一致）
- Debug 构建下 `EventBus` 的队列长度有上限断言
