# 契约：mods

- PluginManager::load(root,disabled) 扫描 mod.json 并用 Windows 运行时链接加载独立 DLL。
- SDK 入口 `orbduel_register(Registrar*)` 注册 Type、Param 与创建/销毁/更新/事件/绘制回调。
- Host 提供实体快照、对象查询、参数读取、状态修改、延迟增删、事件、时间和随机；不暴露 SDL 或宿主内部 World 指针。
- C 导出使用 POD、const char*、实例指针和函数表；DLL 自己销毁自己分配的实例，字符串在回调期间有效。
- 先销毁全部 Simulation/预览实例，再 load/卸载。宿主加载失败保留可用的其他模组并记录原因。
- 不检查哈希、签名或模组版本，不提供自动依赖解析。
- 完整用法和字段含义见 MOD_STUDIO 与 SDK 头文件。
