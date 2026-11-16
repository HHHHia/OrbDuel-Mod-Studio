# 音效 / Audio

两个来源，全部 CC0（可用、可改、可商用、无需署名）：

| 目录 | 来源 | 生成方式 | 用途 |
|---|---|---|---|
| `kenney/impact-sounds/` | [Kenney Impact Sounds](https://kenney.nl/assets/impact-sounds) | 直接下载原包（OGG） | 撞墙、拳头、金属撞击 |
| `kenney/interface-sounds/` | [Kenney Interface Sounds](https://kenney.nl/assets/interface-sounds) | 直接下载原包（OGG） | 按钮、确认、返回 |
| `kenney/sci-fi-sounds/` | [Kenney Sci-fi Sounds](https://kenney.nl/assets/sci-fi-sounds) | 直接下载原包（OGG） | 激光、能量、充能 |
| `kenney/music-jingles/` | [Kenney Music Jingles](https://kenney.nl/assets/music-jingles) | 直接下载原包（OGG） | 回合开始、胜负结算短旋律 |
| `kenney/ui-audio/` | [Kenney UI Audio](https://kenney.nl/assets/ui-audio) | 直接下载原包（OGG） | 按钮、切换 |
| `kenney/rpg-audio/` | [Kenney RPG Audio](https://kenney.nl/assets/rpg-audio) | 直接下载原包（OGG） | 翻书、腰带、轻金属 |
| `kenney/digital-audio/` | [Kenney Digital Audio](https://kenney.nl/assets/digital-audio) | 直接下载原包（OGG） | 激光、升降音 |
| `kenney/casino-audio/` | [Kenney Casino Audio](https://kenney.nl/assets/casino-audio) | 直接下载原包（OGG） | 洗牌、摆放 |
| `kenney/voiceover-pack/` | [Kenney Voiceover Pack](https://kenney.nl/assets/voiceover-pack) | 直接下载原包（OGG） | 男女播报数字/单词 |
| `orbduel/` | 本项目 | `python tools/make_sfx.py` | 对战核心音效，WAV，SDL2 可直接加载 |

## 格式要点

- `kenney/` 下是 **OGG Vorbis**：SDL2 本体不带解码器，要先用 **SDL2_mixer**（或 stb_vorbis）才能读。
- `orbduel/` 下是 **16-bit 44.1 kHz 单声道 WAV**：`SDL_LoadWAV` 直接能读，不需要额外依赖。
- 改名/删文件后重跑 `tools/make_sfx.py` 会原样重建 `orbduel/`（合成脚本是确定性的）。

## 现在放什么

`src/game/Audio.cpp` 用 SDL2 原生音频（不加依赖）播放 `orbduel/` 里的 WAV，只挂最小一组：

| 触发 | 文件 |
|---|---|
| 两球相撞（相对速度 ≥ 420 算重击） | `hit_heavy.wav` / `hit_light.wav` |
| 撞墙、撞障碍（速度 ≥ 120） | `wall_bounce.wav` |
| 一方阵亡 | `ko.wav` |
| 回合结束（有胜者 / 平局） | `round_win.wav` / `round_lose.wav` |
| 开局 | `round_start.wav` |
| 主要按钮、左侧导航 | `ui_click.wav` |

`heal`、`shield`、`charge`、`sudden_death`、`ui_hover`、`ui_deny` 已生成但还没接事件，留给之后用。
试听全部音效：`.\build\bin\orbduel.exe --sfx-test`。

原名、许可证原文见各包内的 `License.txt`（Kenney 的 CC0 声明）。

> `kenney/` 只是本地素材库，**不进 git**（`.gitignore` 里有 `content/audio/kenney/`），约 12.9 MB / 711 个 OGG。
> 仓库里分发的是 `orbduel/` 那 14 个自制 WAV，`tools/make_sfx.py` 可原样重建。

