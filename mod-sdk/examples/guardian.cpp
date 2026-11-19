#include "orbduel_abilities.hpp"
#include <cstdio>
#include <string>
#ifdef ORBDUEL_USER_MOD
#include "mod_config.hpp"
#else
namespace config {
constexpr char typeId[] = "guardian.guard", typeName[] = "护卫召唤";
constexpr float respawn = 5, hpScale = .4f, radiusScale = .6f, attackScale = .5f, speed = 220,
                followDistance = 60, interceptRadius = 180, shieldRadius = 140, shieldAmount = 8,
                shieldDuration = 1.5f, shieldInterval = 3;
} // namespace config
#endif
using namespace orbmod;
namespace {
const EntityApi *entities = nullptr;
const std::string controllerId = std::string(config::typeId) + ".controller";
struct Guard : Ability {
    Guard(const Host *h, Id id) : Ability(h, id, entities) {}
    Cooldown shield;
    void step(float dt) {
        beginStep();
        shield.tick(dt);
        Entity self{}, master{}, enemy{};
        CombatInfo metadata{};
        if (!owner(self) || !info(self.id, metadata) || !get(metadata.owner, master) || !master.alive)
            return;
        float speed = number("speed", config::speed);
        if (nearestEnemy(master.id, enemy, number("interceptRadius", config::interceptRadius)) &&
            enemy.id != self.id)
            moveTowards(self.id, enemy.pos, speed);
        else
            moveTowards(self.id, master.pos, speed, number("followDistance", config::followDistance));
        if (!shield.ready())
            return;
        for (const auto &ally :
             within(self.id, self.pos, number("shieldRadius", config::shieldRadius), Relation::Allies, true))
            host->shield(host->user, ally.id, number("shieldAmount", config::shieldAmount),
                         number("shieldDuration", config::shieldDuration));
        shield.start(std::max(.1f, number("shieldInterval", config::shieldInterval)));
        Event signal{
            "guardian.shield", self.id, master.id, id, self.pos, number("shieldAmount", config::shieldAmount),
            "ability"};
        host->emit(host->user, &signal);
    }
    void draw(const Draw &d) {
        auto o = object();
        d.circle(d.user, o.pos, 20, {100, 225, 200, 200}, 0);
        d.circle(d.user, o.pos, number("shieldRadius", config::shieldRadius), {100, 225, 200, 40}, 0);
    }
};
struct Guardian : Ability {
    Guardian(const Host *h, Id id) : Ability(h, id, entities) {}
    Id child = -1;
    Cooldown respawn;
    void step(float dt) {
        beginStep();
        respawn.tick(dt);
        Entity self{}, live{};
        if (!owner(self) || !respawn.ready() || (get(child, live) && live.alive))
            return;
        EntitySpec spec;
        spec.owner = self.id;
        spec.hpScale = number("hpScale", config::hpScale);
        spec.radiusScale = number("radiusScale", config::radiusScale);
        spec.attackScale = number("attackScale", config::attackScale);
        spec.pos = {self.pos.x + self.radius * (1 + spec.radiusScale) + 8, self.pos.y};
        child = create(spec);
        if (child < 0)
            return;
        char params[512];
        std::snprintf(params, sizeof(params),
                      "{\"speed\":%.6g,\"followDistance\":%.6g,\"interceptRadius\":%.6g,\"shieldRadius\":%."
                      "6g,\"shieldAmount\":%.6g,\"shieldDuration\":%.6g,\"shieldInterval\":%.6g}",
                      double(number("speed", config::speed)),
                      double(number("followDistance", config::followDistance)),
                      double(number("interceptRadius", config::interceptRadius)),
                      double(number("shieldRadius", config::shieldRadius)),
                      double(number("shieldAmount", config::shieldAmount)),
                      double(number("shieldDuration", config::shieldDuration)),
                      double(number("shieldInterval", config::shieldInterval)));
        if (bind(child, controllerId.c_str(), params) < 0) {
            if (api)
                api->destroy(host->user, child);
            child = -1;
        }
        respawn.start(std::max(.1f, number("respawn", config::respawn)));
    }
    void draw(const Draw &d) {
        auto o = object();
        d.circle(d.user, o.pos, 14, {100, 225, 200, 160}, 0);
    }
};
} // namespace
ORBDUEL_MOD_EXPORT void orbduel_set_entity_api(const EntityApi *api) {
    entities = api;
}
ORBDUEL_MOD_EXPORT void orbduel_register(Registrar *r) {
    static const Param params[] = {
        {"respawn", "补充护卫间隔", ParamKind::Number, config::respawn, .1f, 30, "",
         "每个能力只维持一个护卫"},
        {"hpScale", "护卫生命倍率", ParamKind::Number, config::hpScale, .01f, 3, "", ""},
        {"radiusScale", "护卫体型倍率", ParamKind::Number, config::radiusScale, .1f, 2, "", ""},
        {"attackScale", "护卫攻击倍率", ParamKind::Number, config::attackScale, 0, 3, "", ""},
        {"speed", "移动速度", ParamKind::Number, config::speed, 0, 1800, "", ""},
        {"followDistance", "跟随停止距离", ParamKind::Number, config::followDistance, 0, 300, "", ""},
        {"interceptRadius", "主人周围拦截范围", ParamKind::Number, config::interceptRadius, 0, 500, "", ""},
        {"shieldRadius", "护卫保护范围", ParamKind::Number, config::shieldRadius, 0, 500, "",
         "只保护同队及自身"},
        {"shieldAmount", "护盾量", ParamKind::Number, config::shieldAmount, 0, 100, "", ""},
        {"shieldDuration", "护盾时长", ParamKind::Number, config::shieldDuration, .1f, 20, "", ""},
        {"shieldInterval", "保护间隔", ParamKind::Number, config::shieldInterval, .1f, 30, "", ""}};
    auto t = behavior<Guardian>(config::typeId, config::typeName, "召唤跟随主人、拦截敌人并保护友军的护卫。",
                                params, 11, 0, {100, 225, 200, 255});
    r->add(r->user, &t);
    auto controller = behavior<Guard>(controllerId.c_str(), "护卫控制器", "由护卫召唤能力自动绑定。", params,
                                      11, -1, {100, 225, 200, 255});
    r->add(r->user, &controller);
}
