# 契约：mods

- PluginManager::load(root,disabled,extraRoots) 扫描 mod.json 并按平台加载原生模组；新增参数可省略。
- SDK 入口 `orbduel_register(Registrar*)` 注册 Type、Param 与创建/销毁/更新/事件/绘制回调。
- Host 提供实体快照、对象查询、参数读取、状态修改、延迟增删、事件、时间和随机；不暴露 SDL 或宿主内部 World 指针。
- C 导出使用 POD、const char*、实例指针和函数表；DLL 自己销毁自己分配的实例，字符串在回调期间有效。
- 先销毁全部 Simulation/预览实例，再 load/卸载。宿主加载失败保留可用的其他模组并记录原因。
- 不检查哈希、签名或模组版本，不提供自动依赖解析。
- 完整用法和字段含义见 MOD_STUDIO 与 SDK 头文件。
- 加载时复制主库与同目录原生库到 .orbduel-cache 下的本次加载目录；原安装文件保持可写。素材根目录仍是原模组目录。
- clear 在卸载动态库后，仅删除本实例记录的缓存文件及空目录；不递归删除模组或用户文件。不做比赛中卸载，调用方仍需先销毁所有实例。
- 新手模板由 mod.json 生成类型名与默认参数；new-mod.ps1 复制自包含 SDK 工程，build-install.ps1 只安装运行文件、SDK 许可证与 assets。游戏路径仅存本地 .orbduel-user.json。

- 可选头 orbduel_entities.hpp 定义 EntitySpec、CombatInfo 与 EntityApi（create/destroy/get/info/bind/unbind）。PluginManager::setEntityApi 在 load 前设置；若 DLL 导出 orbduel_set_entity_api 则在 register 前注入，未设置传 nullptr。函数表必须覆盖模组生命周期。旧 Host/Entity/Type/Registrar 布局不改。
- new-mod.ps1 -Kind summon 复制召唤示例，默认 vortex 保留。独立工程携带自包含 SDK 头；mod.json 生成分身参数默认值，沿用一键构建安装、重新加载和公开打包白名单。

## macOS 适配与用户模组

- load(root,disabled,extraRoots) 保持旧参数兼容；额外目录按顺序覆盖同 ID 模组，禁用 ID 不回退到内置版本。实例仍须在 load 前全部销毁。未指定 extraRoots 时使用传入的主模组目录。
- Windows 使用 LoadLibraryExW/GetProcAddress/FreeLibrary 与 .dll；macOS 使用 dlopen/dlsym/dlclose 与 .dylib；缓存目录放在最后一个可写用户根目录，素材仍读取原安装目录。SDK 的 Host/Entity/Type/Registrar 布局不改；各系统分别构建原生库，不尝试运行另一系统的二进制。
- CMake 在构建输出中写入当前平台原生库文件名；不改源 mod.json 的 Windows 默认字段。new-mod.cmake 复制自包含 C++ 工程；install-mod.cmake 只安装本次指定包，不扫描旧构建包。Windows PS/.cmd 流程保留，Mac 增加 Shell 入口。
- 适配代码未在 macOS 编译或实机验证，按本次任务明确排除；Windows portable_workflow 验证跨平台生成/安装器与用户覆盖机制。无哈希、签名或版本锁。

## C++ 自建能力作者层

- orbduel_abilities.hpp 提供每实例 Ability、Cooldown、Interval；不改变旧 Host/Entity/Type/Registrar/EntityApi 布局。Ability 包装主人/稳定 ID 查询、事件筛选、队伍范围/最近敌人、定向移动和自有资源清理。
- create/bind/spawn 登记本能力创建的资源，beginStep 每步调用一次，保留同一步可能尚未提交的资源并清理失效记录，析构或 cleanup 把清理请求交回安全边界；不负责作者绕过包装直接创建的对象。Ability 不可复制。
- Type.placement=-1 为仅运行时类型：编辑器/工坊隐藏、不进入 generationTypes；0/1/2 旧含义不变。原生加载、查找及 bind 仍可使用内部类型。
- 新增 ability/rewind/guardian 三种独立工程模板，两种 C++ DLL 示例和试验地图；没有宿主按能力名称分派的新逻辑。详见 CUSTOM_ABILITIES。
