# 契约：render

- Renderer/AssetCache 保留 SDL2 资源管理接口；贴图 reload 先加载候选，失败保留旧贴图。
- Themes 提供五种纯表现风格、配色和 drawTheme；表现时间与对局时间分离，不消费比赛随机数。
- themePalette、themeName 使用 cosmos/dungeon/neon/lava/ice；drawTheme 支持 Palette 覆盖。
- 模组 Draw 函数表由 game 层适配 ImDrawList，坐标统一转换到编辑器视口；图片相对当前模组目录。
- 只有 game/render 依赖 SDL 与 ImGui；渲染不改变比赛状态。
