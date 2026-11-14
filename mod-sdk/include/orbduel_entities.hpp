#pragma once
// 可选战斗实体扩展；不改变原有 Host/Entity/Type 的二进制布局。
#include "orbduel_mod.hpp"

namespace orbmod {
struct EntitySpec {
    const char *character = nullptr; // 角色 ID；空指针表示复制 owner 的角色定义。
    Id owner = -1;
    int team = -1; // -1 继承 owner；无 owner 时为中立，和所有队伍敌对。
    Vec pos{}, velocity{};
    float hpScale = 1, radiusScale = 1, attackScale = 1;
    float lifetime = 0; // 秒；0 表示不自动到期。
    float contact = 1;  // 挨撞系数，沿用地图与角色的接触伤害规则。
    int copySkills = 0; // 非零时复制角色自带技能；默认仅用绑定的模组能力。
};
struct CombatInfo {
    int team;
    Id owner;
    int primary;
    float lifetime; // 剩余秒数；0 表示无期限。
};
struct EntityApi {
    // 创建/销毁/绑定在固定步边界生效。ID 不复用；创建失败返回 -1。
    Id (*create)(void *simulation, const EntitySpec *);
    void (*destroy)(void *simulation, Id entity);
    int (*get)(void *simulation, Id entity, Entity *out);
    int (*info)(void *simulation, Id entity, CombatInfo *out);
    Id (*bind)(void *simulation, Id entity, const char *type, const char *paramsJson);
    void (*unbind)(void *simulation, Id ability);
};
// 模组可导出 orbduel_set_entity_api(const EntityApi*)，宿主在 register 前注入。
using EntityApiReceiver = void (*)(const EntityApi *);
} // namespace orbmod
