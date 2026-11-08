# 契约：render

**Owner**：B　**依赖**：`core`、SDL2　**被依赖**：`game`

## 职责

- 窗口与主渲染循环（`Renderer`）
- 纹理加载与缓存（`AssetCache`，支持热重载）
- 粒子 / 拖尾 / 震屏 / hit-stop（`Fx`）
- 调试绘制（`DebugDraw`）

## 不负责

- 不推进物理、不改任何游戏状态
- 不读 JSON、不认识 `CharacterDef`
- 不决定"要不要播特效" → 由事件驱动

## 公开接口

| 文件 | 内容 |
|---|---|
| `render/Renderer.hpp` | `init/shutdown/beginFrame/endFrame`、`drawSprite`、`drawCircle`、`drawRing`、`drawLine`、`drawText`、`size()` |
| `render/AssetCache.hpp` | `texture(path)`、`reload(path)`、`reloadAll()`、`errors()` |
| `render/Fx.hpp` | `bind(EventBus&)`、`update(dt)`、`draw(Renderer&)`、`shake()`、`requestHitStop()` |
| `render/DebugDraw.hpp` | `toggle(layer)`、`draw(Renderer&, const World&, const Match&)` |

## 不变量

1. `Renderer` 实例全局唯一（SDL 的渲染器限制），由 `Game` 持有。
2. `AssetCache::reload` 失败时保留旧纹理，不返回空指针。
3. `Fx` 的粒子数上限固定（默认 2048），满了覆盖最老的。
4. `hit_stop` 只影响"本帧是否推进物理"，由 `Game` 读取，不由 `Fx` 直接改时钟。
5. 所有绘制调用必须在 `beginFrame` / `endFrame` 之间。

## 禁止

- 在 `draw*` 里修改任何 `Body` / `Match`
- 让 `physics` / `content` / `match` include `<SDL.h>`
- 在 `Fx` 里做碰撞检测

## 验收

- `--selftest` 能创建隐藏窗口并干净退出（`--window` 模式）
- 连续热重载 20 次贴图无泄漏（任务管理器看内存不涨）
- 关闭窗口后无 SDL 报错；exe 旁边必须有 `SDL2.dll` 等 3 个 DLL
