# 用 C++ 自建能力

能力的算法位于独立模组的 C++ 文件中。宿主提供实体、运动、伤害和生命周期操作；模组自己定义何时触发、选谁、保存什么状态、产生什么效果。新增回溯和护卫没有给引擎添加按技能名称分派的分支。

## 从作者模板开始

Windows：

~~~powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\mod-sdk\new-mod.ps1 -Id my-ability -Kind ability -Name "我的能力"
~~~

Mac 源码入口（本次仍不做 Mac 编译或实机验证）：

~~~bash
bash mod-sdk/new-mod.sh --id my-ability --kind ability --name "我的能力"
~~~

进入生成工程，打开 my_mod.cpp。模板中的 MyAbility 是你的 C++ 类：step 更新状态，event 定义触发与效果，draw 绘制反馈，底部注册类型和参数。初始例子为“受到生命伤害后给附近友军护盾”，作为可运行起点；修改 event 的代码即可写自己的算法。只改 JSON 可以调参，新的行为仍由你写 C++。

mod.json 的 defaults 调整模板已有的 cooldown/shieldAmount/shieldDuration/shieldRadius。新增可调参数时，在 C++ 的 Param 数组声明；参数面板自动出现，用 number() 读取。若还想从 mod.json 生成新的编译默认值，同步修改工程 CMakeLists.txt 与 mod_config.hpp.in。

Windows 双击 build-install.cmd；Mac 使用 Build-Install.command 或 bash build-install.sh。回游戏工坊重新加载，编辑器放置你的能力，**把“附着”改为左方或右方角色**，再测试。未附着的能力只显示标记，不会自动选择一个主人。新默认值影响新对象，旧对象保留自己的参数。

## C++ 作者辅助层

包含 orbduel_abilities.hpp；模组仍导出 orbduel_register，并通过可选 orbduel_set_entity_api 接收实体函数表。Ability 只包装原 Host 和 EntityApi，没有扩展旧函数表或改变其布局。

| 工具 | 用途 |
|---|---|
| ownerId() / owner(snapshot) | 读取本能力所属实体；owner() 仅对存活实体成功 |
| get(id,snapshot) / info(id,metadata) | 按稳定 ID 查询，避免把 ID 当枚举下标 |
| hurt(event) / hit(event) / died(event) | 筛选所属实体的生命伤害、接触和死亡事件 |
| within(source,center,radius,relation,includeSelf) | 取得范围内存活实体；支持任意、友军、敌军 |
| nearestEnemy(source,out,radius) | 查最近敌人，等距时按稳定 ID 决定 |
| moveTowards(id,point,speed,stopDistance) | 设置朝目标移动的速度，进入停止距离后停下；碰撞仍由物理处理 |
| Cooldown / Interval | 每实例冷却和周期触发；在 step 中推进 |
| create / bind / spawn | 创建由本能力管理的实体、绑定实例或模组对象 |
| beginStep() / cleanup() | 步首剔除失效记录；能力销毁时自动清理自己创建的资源 |

自己创建资源时使用 Ability 的包装，才能自动登记清理。直接调用原始 Host/EntityApi 时，作者自行管理其生命周期。cleanup 可重复调用，增删仍在宿主的安全边界提交，不在事件遍历中销毁实例。不能复制 Ability 对象，避免两个实例重复认领同一资源。

友军/敌军查询使用实体队伍；负队伍沿用引擎的中立语义。没有可选实体接口时，按 ID 查询可退回旧 Host 枚举；未知队伍不猜测为友军或敌军。不增加哈希、签名和版本锁。

## 时间回溯示例

内置类型 rewind.recall；独立工程使用 -Kind rewind。

受伤后生命比例低于 threshold 且冷却结束，恢复本实体 lookback 秒前的位置、速度，以及当时较高的生命值。每 0.1 秒采样，最多保留十秒窗口；不足窗口时使用最早记录。生效后清空旧历史并开始冷却，避免重复消费同一份旧生命快照。

只回溯这个实体的位置、速度和生命；不回退整局时钟、其他实体、护盾或内置技能冷却。致死伤害不复活，护盾完全吸收的伤害不触发。位置仍经过宿主当前边界/障碍处理。蓝色连线和光圈提供反馈。

## 护卫召唤示例

内置类型 guardian.guard；独立工程使用 -Kind guardian。

每个能力维护一个护卫：远离主人时跟随，主人附近有敌人时拦截，周期性给护卫周围友军（含自身）提供护盾。主人、护卫和能力各有独立状态；护卫 HP 死亡后按补充间隔重新生成，主人死亡或移除召唤能力时清理。

控制器是该 DLL 注册的内部类型，通过 bind 自动附着到护卫。Type.placement=-1 表示只在运行时使用，不出现在手工放置列表和随机生成候选里；旧 0/1/2 含义保留。两个新增示例都不进入默认地图随机候选，原竞技测量范围不变；自定义能力玩法需要自己的平衡判断。

## 直接试玩与分享

在编辑器打开 content/studio/custom-abilities.json，双方各附着回溯与护卫。测试结束返回编辑器，调整参数再试；也可以把新能力绑定到自己创建的额外实体。

分享独立工程源码可在另一平台重新构建；运行成品分享 build/package/<id>/（Mac 是 build-macos/package/<id>/）。Windows DLL 和 Mac dylib 分开编译。旧 SDK 模组继续可加载；仍须先销毁对局与预览再卸载模组。

本次仅在 Windows 运行编译与回归，包含真实 DLL 载入、回溯恢复/冷却/死亡边界、护卫跟随/拦截/保护/补充、能力移除清理，以及三种作者模板的独立构建安装。Mac 的入口只随源码提供，未做编译或实机验证。
