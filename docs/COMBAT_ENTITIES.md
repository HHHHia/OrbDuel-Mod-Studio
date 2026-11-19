# 战斗实体与分身模组

现在模组可以创建真正的战斗实体：独立位置、速度、碰撞体、HP、护盾、内置技能和模组能力。分身示例的追踪与召唤逻辑都在独立 DLL 中，引擎不识别“分身”这种玩法名称。

## 先试内置示例

构建后打开游戏，在地图编辑器打开 `content/studio/summons.json`，点击测试。双方各有一个召唤器，默认每两秒补充一个分身、每个召唤器最多两个，分身存活八秒。小实体上方显示自己的血条。它们会追踪最近的敌人，与墙、障碍及其他实体碰撞，受到普通伤害后死亡。

也可在任意地图手工放置“回声分身”，选择所属角色，修改间隔、数量、生命、体型、攻击和速度。附着到角色时，附着对象优先于“所属角色”参数。

**沿用原接触规则**：普通伤害倍率为地图 `damageScale` × 挨撞实体的 `contact`，再按攻击方碰撞前正向速度计算有界冲击倍率。分身默认 `contact=1`；想让双方普通碰撞都能造成伤害，在地图规则中把碰撞伤害系数设为 1。示例地图已经设置好了。沿用竞技平衡版的角色参数；显式为 0 的碰撞倍率仍会关闭普通接触伤害。

## 新手：复制、调参、安装

在游戏目录打开 PowerShell：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\mod-sdk\new-mod.ps1 -Id my-summons -Kind summon -Name "我的分身"
```

进入生成的 `user/mod-projects/my-summons`，编辑 `mod.json` 的 `defaults`：

```json
{
  "interval": 1.5,
  "maxAlive": 3,
  "lifetime": 6,
  "hpScale": 0.25,
  "radiusScale": 0.5,
  "attackScale": 0.4,
  "speed": 360
}
```

这段是 `defaults` 的内容，保留文件中的 `id`、`name`、`library`、`typeId`、`typeName`。生命和攻击相对所属角色缩放，质量随体型面积缩放；`lifetime=0` 表示不自动消失。原旋风模板仍是默认选项，省略 `-Kind summon` 即可创建旋风。

双击 `build-install.cmd`，第一次选择游戏的 `orbduel.exe`。构建安装成功后回游戏点击“重新加载模组和内容”，在编辑器放置“我的分身”并测试。第二次改参数后仍是双击同一个文件，不需要退出新版游戏。新默认值影响新放置的召唤器；旧对象保留编辑过的值，可直接在右侧属性中修改。

分享 `build/package/my-summons/`，无需复制宿主源码。不要分享 `.orbduel-user.json` 或整个 build 目录，它们可能带本机安装位置。详细工具链与故障处理见 [第一次制作模组](FIRST_MOD.md)。

## 作者：六个扩展接口

包含 `orbduel_entities.hpp`，接收可选函数表：

```cpp
using namespace orbmod;
static const EntityApi *entities = nullptr;
ORBDUEL_MOD_EXPORT void orbduel_set_entity_api(const EntityApi *api) {
    entities = api;
}
```

在一个 `Behavior` 的回调中创建分身并绑定任意已注册类型：

```cpp
if (!entities) return;
EntitySpec spec;
spec.owner = object().owner; // 此例能力须先附着到角色。
spec.pos = object().pos;
spec.hpScale = .3f;
spec.lifetime = 8;
Id child = entities->create(host->user, &spec);
if (child >= 0)
    entities->bind(host->user, child, "my-summons.echo", "{\"speed\":360}");
```

`"my-summons.echo"` 是上述复制命令生成的类型名；换成你实际注册的名称。

| 接口 | 含义 |
|---|---|
| `create(user, &spec)` | 返回预留实体 ID；失败为 -1，下一固定步边界提交 |
| `destroy(user, entity)` | 清理额外实体及后代、绑定能力；重复调用无害，主角色不允许用此接口移除 |
| `get(user, entity, &snapshot)` | 按 ID 查询只读快照；未提交或已移除返回 0 |
| `info(user, entity, &info)` | 查询 team、owner、是否主角色、剩余寿命 |
| `bind(user, entity, type, json)` | 把已有类型实例绑定到实体，返回能力对象 ID；可在 create 后立刻调用 |
| `unbind(user, ability)` | 只移除这一个能力实例，不伤害实体 |

`EntitySpec.character` 可指定已加载的角色 ID；留空复制 owner 的完整角色快照。`team=-1` 继承 owner；没有 owner 时需要指定 character，此时 -1 表示与所有其他实体敌对。`copySkills=0` 默认不复制内置技能，非零则复制角色技能，包括贯穿、蓄力、引力和通用修饰器。绑定能力数量不限于一个，也可以给主角色绑定。

位置、速度、受伤、治疗、护盾继续使用原 `Host`。`Host::spawn/remove` 操作的是模组对象，`EntityApi::create/destroy` 操作的是战斗实体，两类 ID 属于各自的空间。`Host::entityAt` 的参数是枚举下标，返回的 `Entity.id` 才是稳定 ID；删除实体后不能拿 ID 当下标。

能力读取 `object().owner` 获得绑定的战斗实体 ID。对象位置在每步回调前以及物理后跟随实体。完整例子在 `mod-sdk/examples/summon.cpp`：同一种 Type 放在地图/主角色上执行召唤，绑定到额外实体上执行追踪。追踪代码可以替换为巡逻、逃跑、冲锋或护卫，无需改引擎。

## 生命周期与规则

- 同队实体仍有物理碰撞，普通命中不会造成友伤、消耗重击或打断友方蓄力。范围技能筛选敌队，单目标内置技能选择最近的存活敌人。模组显式调用 `Host::damage` 时由作者选择目标，可以实现友伤规则。
- `death`：HP 降到零，仅触发一次；回调内仍可 `get` 到 `alive=0` 的快照。实体没有复活接口。
- `entity_spawned`：`a` 为实体 ID，`b` 为 owner；实体和本边界提交的绑定能力已可查询。
- `entity_removed`：移除前通知，`a` 为实体 ID、`b` 为 owner，cause 为 `destroyed/death/lifetime/owner_removed/round_end`。通知后销毁绑定能力并回收实体。取消尚未出生的实体不产生出生或移除事件。
- 附属实体 HP 死亡、到期、显式销毁、所有者死亡/移除或对局结束，都清理相应能力和后代。寿命结束与主动清理不伪造 HP 死亡事件。比赛胜负、计时比较及缩圈终局仍只看最初两名主角色。
- 增删请求在步首/步末安全边界生效；一个回调期间枚举集合保持稳定。构造/销毁回调产生的请求留到下一边界。ID 本局不复用，已移除实体不会留下持续增长的空槽。
- 战斗实体是运行状态，不写回编辑草稿或分享码。地图保存召唤器及参数；相同内容、模组、种子和规则按固定步重现召唤过程。

旧 SDK 的 `Host/Entity/Type/Registrar` 布局保持不变；旧 DLL 无需重编译。新模组要判空扩展指针；旧游戏忽略扩展入口时，分身示例安全停用召唤。宿主集成方须在 `PluginManager::load` 前设置 `Simulation::entityApi()`，且先销毁对局再卸载 DLL。不添加哈希、签名或版本锁。

也可用 [C++ 作者辅助头](CUSTOM_ABILITIES.md) 封装 EntityApi 的查询、绑定与清理。从 `-Kind guardian` 开始可查看如何给召唤物绑定独立控制器。

地图 fields 对全部战斗实体生效；affect=0 作用于所有队伍，1/2 分别筛选左/右队伍及其召唤物。熔岩池 DLL 的“所有球”也包含额外实体。
