#include "mod_config.hpp"
#include "orbduel_abilities.hpp"
using namespace orbmod;
namespace {
const EntityApi *entities = nullptr;
// 从这个 C++ 类开始写自己的算法：触发、目标筛选、效果和冷却都属于模组。
struct MyAbility : Ability {
    MyAbility(const Host *h, Id id) : Ability(h, id, entities) {}
    Cooldown cooldown;
    void step(float dt) {
        beginStep();
        cooldown.tick(dt);
    }
    void event(const Event &e) {
        Entity self{};
        if (!hurt(e) || !cooldown.ready() || !owner(self))
            return;
        for (const auto &ally :
             within(self.id, self.pos, number("shieldRadius", config::shieldRadius), Relation::Allies, true))
            host->shield(host->user, ally.id, number("shieldAmount", config::shieldAmount),
                         number("shieldDuration", config::shieldDuration));
        cooldown.start(number("cooldown", config::cooldown));
    }
    void draw(const Draw &d) {
        auto o = object();
        d.circle(d.user, o.pos, 14, {100, 220, 255, 180}, 0);
    }
};
} // namespace
ORBDUEL_MOD_EXPORT void orbduel_set_entity_api(const EntityApi *api) {
    entities = api;
}
ORBDUEL_MOD_EXPORT void orbduel_register(Registrar *r) {
    static const Param params[] = {
        {"cooldown", "触发冷却", ParamKind::Number, config::cooldown, .1f, 30, "",
         "收到自己的受伤事件才触发"},
        {"shieldAmount", "护盾量", ParamKind::Number, config::shieldAmount, 0, 100, "",
         "修改 event 中的 C++ 代码可替换为新机制"},
        {"shieldDuration", "护盾时长", ParamKind::Number, config::shieldDuration, .1f, 20, "", ""},
        {"shieldRadius", "友军范围", ParamKind::Number, config::shieldRadius, 0, 500, "",
         "包含自己，排除敌人"}};
    auto t =
        behavior<MyAbility>(config::typeId, config::typeName, "C++ 能力作者起步模板：受伤后保护附近友军。",
                            params, 4, 0, {100, 220, 255, 255});
    r->add(r->user, &t);
}
