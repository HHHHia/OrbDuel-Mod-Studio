# 素材登记 / Asset register

> 公开仓库，**只收 CC0、公有领域或自制素材**。每加一个文件就在这里登记一行。
> 从游戏里抠的图、来源不明的网图一律不许进仓库。

## 当前素材

| 文件 | 类型 | 来源 | 许可证 | 备注 |
|---|---|---|---|---|
| `characters/lancer/skin.png` | 球体贴图 | 本项目生成 | CC0（自制） | **占位图**，正式美术替换后删除本行 |
| `characters/singularity/skin.png` | 球体贴图 | 本项目生成（Pillow 脚本，未入库） | CC0（自制） | 黑洞：黑核 + 紫红吸积环，正式美术替换后删除本行 |
| `characters/wooden-dummy/skin.png` | 球体贴图 | 本项目生成（同上） | CC0（自制） | 木人桩：木色圆盘 + 红靶心，同上 |
| `fonts/PressStart2P-Regular.ttf` | 字体 | https://github.com/google/fonts/tree/main/ofl/pressstart2p | SIL OFL 1.1 | HUD 文字；许可证全文见 `fonts/OFL.txt` |

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
