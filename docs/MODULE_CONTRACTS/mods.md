# 契约：mods

- PluginManager::load(root,disabled) 扫描 mod.json 并用 Windows 运行时链接加载独立 DLL。
- SDK 入口 `orbduel_register(Registrar*)` 注册 Type、Param 与创建/销毁/更新/事件/绘制回调。
- Host 提供实体快照、对象查询、参数读取、状态修改、延迟增删、事件、时间和随机；不暴露 SDL 或宿主内部 World 指针。
- C 导出使用 POD、const char*、实例指针和函数表；DLL 自己销毁自己分配的实例，字符串在回调期间有效。
- 先销毁全部 Simulation/预览实例，再 load/卸载。宿主加载失败保留可用的其他模组并记录原因。
- 不检查哈希、签名或模组版本，不提供自动依赖解析。
- 完整用法和字段含义见 MOD_STUDIO 与 SDK 头文件。
- 加载时复制主 DLL 与同目录 DLL 到 mods/.orbduel-cache 下的本次加载目录；原安装文件保持可写。素材根目录仍是原模组目录。
- clear 在 FreeLibrary 后，仅删除本实例记录的缓存文件及空目录；不递归删除模组或用户文件。不做比赛中卸载，调用方仍需先销毁所有实例。
- 新手模板由 mod.json 生成类型名与默认参数；new-mod.ps1 复制自包含 SDK 工程，build-install.ps1 只安装运行文件、SDK 许可证与 assets。游戏路径仅存本地 .orbduel-user.json。

- 可选头 orbduel_entities.hpp 定义 EntitySpec、CombatInfo 与 EntityApi（create/destroy/get/info/bind/unbind）。PluginManager::setEntityApi 在 load 前设置；若 DLL 导出 orbduel_set_entity_api 则在 register 前注入，未设置传 nullptr。函数表必须覆盖模组生命周期。旧 Host/Entity/Type/Registrar 布局不改。
- new-mod.ps1 -Kind summon 复制召唤示例，默认 vortex 保留。独立工程携带两份 SDK 头；mod.json 生成分身参数默认值，沿用一键构建安装、重新加载和公开打包白名单。
