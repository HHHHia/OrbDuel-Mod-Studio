# 素材登记 / Asset register

> 公开仓库，**只收 CC0、公有领域或自制素材**。每加一个文件就在这里登记一行。
> 从游戏里抠的图、来源不明的网图一律不许进仓库。

## 当前素材

工作台新增的星空、地下城、霓虹都市、熔岩遗迹、冰晶圣殿背景和装饰由本项目 C++ 程序化绘制，无外部场景图片。参数控件由 Dear ImGui（MIT）提供。中文界面使用系统已安装的 Microsoft YaHei 字体，不在发布包中复制或分发该系统字体。

| 文件 | 类型 | 来源 | 许可证 | 备注 |
|---|---|---|---|---|
| `characters/blaze/skin.png` | 球体贴图 | 原项目自制占位图 | CC0（自制） | 工作台角色，默认使用程序化徽记 |
| `characters/bulwark/skin.png` | 球体贴图 | 原项目自制占位图 | CC0（自制） | 工作台角色，默认使用程序化徽记 |
| `characters/lancer/skin.png` | 球体贴图 | 本项目生成 | CC0（自制） | **占位图**，正式美术替换后删除本行 |
| `characters/singularity/skin.png` | 球体贴图 | 本项目生成（Pillow 脚本，未入库） | CC0（自制） | 黑洞：黑核 + 紫红吸积环，正式美术替换后删除本行 |
| `characters/wooden-dummy/skin.png` | 球体贴图 | 本项目生成（同上） | CC0（自制） | 木人桩：木色圆盘 + 红靶心，同上 |
| `fonts/PressStart2P-Regular.ttf` | 字体 | https://github.com/google/fonts/tree/main/ofl/pressstart2p | SIL OFL 1.1 | HUD 文字；许可证全文见 `fonts/OFL.txt` |
| `audio/orbduel/*.wav` | 音效 | 本项目 `tools/make_sfx.py` 波形合成 | CC0（自制） | 16-bit 44.1 kHz 单声道；SDL2 原生播放，不需 SDL_mixer |
| `audio/kenney/**` | 音效素材库 | Kenney（https://kenney.nl/assets） | CC0 | OGG Vorbis，暂未接入播放；**本地素材库，不进 git**，不随仓库分发 |

## 推荐素材站（都是 CC0 / 免费商用）

- https://kenney.nl/assets — 2D/UI 全套，CC0，风格统一
- https://opengameart.org — 筛选 License: CC0
- https://itch.io/game-assets/free — 注意逐个看许可证

## 登记格式

新增一行：`| 相对路径 | 贴图/音效/字体 | 作者 + 链接 | 许可证 | 备注 |`

导入前确认三件事：
1. 许可证允许**再分发**（CC0、CC-BY 可以；CC-BY-NC 不行，作业仓库也算分发）；
2. 署名要求（CC-BY 必须在 README 或本文件里署名）；
3. 图片尺寸与透明通道符合 `docs/CONTENT_FORMAT.md`。

## 六角色徽记

flame、hex、chevron、crystal、spark、leaf 由本项目 C++ 绘制，为自制程序化图形。四个新增角色目录中的 skin.png 复用原自制烈焰占位素材；启用 pattern 时实际显示对应徽记，无外部角色图片。
