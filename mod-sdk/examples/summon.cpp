#include "orbduel_entities.hpp"
#include <algorithm>
#include <cstdio>
#include <vector>
#ifdef ORBDUEL_USER_MOD
#include "mod_config.hpp"
#else
namespace config {
constexpr char typeId[] = "summon.echo", typeName[] = "回声分身";
constexpr float interval = 2, maxAlive = 2, lifetime = 8, hpScale = .35f, radiusScale = .65f,
                attackScale = .5f, speed = 280;
} // namespace config
#endif
using namespace orbmod;
namespace {
const EntityApi *entities = nullptr;
struct Summon : Behavior {
    using Behavior::Behavior;
    float timer = 0;
    std::vector<Id> children;
    void step(float dt) {
        if (!entities)
            return;
        auto o = object();
        CombatInfo ownerInfo{};
        if (o.owner >= 0 && entities->info(host->user, o.owner, &ownerInfo) && !ownerInfo.primary) {
            // 同一种能力绑定到分身后负责追踪，避免为每种 AI 增加引擎分支。
            Entity self{};
            if (!entities->get(host->user, o.owner, &self) || !self.alive)
                return;
            Entity nearest{};
            float best = 1e30f;
            for (int i = 0; i < count(); ++i) {
                auto e = entity(i);
                CombatInfo info{};
                if (!e.alive || e.id == self.id || !entities->info(host->user, e.id, &info) ||
                    (info.team >= 0 && info.team == ownerInfo.team))
                    continue;
                float dist = distance(self.pos, e.pos);
                if (dist < best) {
                    best = dist;
                    nearest = e;
                }
            }
            if (best < 1e29f && best > .01f) {
                float speed = std::max(0.f, number("speed", config::speed));
                host->setVelocity(host->user, self.id,
                                  {(nearest.pos.x - self.pos.x) / best * speed,
                                   (nearest.pos.y - self.pos.y) / best * speed});
            }
            return;
        }
        timer -= dt;
        children.erase(std::remove_if(children.begin(), children.end(),
                                      [&](Id id) {
                                          Entity e{};
                                          return !entities->get(host->user, id, &e) || !e.alive;
                                      }),
                       children.end());
        if (timer > 0 || int(children.size()) >= std::max(0, int(number("maxAlive", config::maxAlive))))
            return;
        Id owner = o.owner >= 0 ? o.owner : int(number("owner", 0));
        Entity source{};
        if (!entities->get(host->user, owner, &source) || !source.alive)
            return;
        EntitySpec spec;
        spec.owner = owner;
        spec.hpScale = number("hpScale", config::hpScale);
        spec.radiusScale = number("radiusScale", config::radiusScale);
        spec.attackScale = number("attackScale", config::attackScale);
        spec.lifetime = number("lifetime", config::lifetime);
        spec.copySkills = number("copySkills", 0) != 0;
        Vec dir = direction(host->random(host->user, 0, 360));
        float offset = source.radius * (1 + spec.radiusScale) + 6;
        spec.pos = {source.pos.x + dir.x * offset, source.pos.y + dir.y * offset};
        Id child = entities->create(host->user, &spec);
        if (child < 0)
            return;
        char params[96];
        std::snprintf(params, sizeof(params), "{\"speed\":%.6g}", double(number("speed", config::speed)));
        entities->bind(host->user, child, config::typeId, params);
        children.push_back(child);
        timer = std::max(.05f, number("interval", config::interval));
    }
    void draw(const Draw &d) {
        auto o = object();
        if (o.owner >= 0)
            d.circle(d.user, o.pos, 11, {160, 240, 255, 200}, 0);
        else {
            d.circle(d.user, o.pos, 22, {160, 240, 255, 180}, 0);
            d.line(d.user, {o.pos.x - 12, o.pos.y}, {o.pos.x + 12, o.pos.y}, {160, 240, 255, 180}, 2);
        }
    }
};
} // namespace
ORBDUEL_MOD_EXPORT void orbduel_set_entity_api(const EntityApi *api) {
    entities = api;
}
ORBDUEL_MOD_EXPORT void orbduel_register(Registrar *r) {
    static const Param params[] = {
        {"owner", "所属角色", ParamKind::Choice, 0, 0, 1, "左方|右方", "绑定到角色时使用绑定对象作为所有者"},
        {"interval", "召唤间隔（秒）", ParamKind::Number, config::interval, .05f, 30, "",
         "低于上限时再次召唤"},
        {"maxAlive", "同时存活上限", ParamKind::Integer, config::maxAlive, 0, 20, "", "每个召唤器单独计数"},
        {"lifetime", "存活时间（秒）", ParamKind::Number, config::lifetime, 0, 60, "", "0 表示不自动消失"},
        {"hpScale", "生命倍率", ParamKind::Number, config::hpScale, .01f, 5, "", "相对于所属角色"},
        {"radiusScale", "体型倍率", ParamKind::Number, config::radiusScale, .1f, 3, "", "质量按面积缩放"},
        {"attackScale", "攻击倍率", ParamKind::Number, config::attackScale, 0, 5, "", "沿用接触伤害规则"},
        {"speed", "追踪速度", ParamKind::Number, config::speed, 0, 1800, "", "会与墙、障碍和其他实体碰撞"},
        {"copySkills", "复制内置技能", ParamKind::Toggle, 0, 0, 1, "", "额外复制角色技能，例如贯穿或引力井"}};
    auto t = behavior<Summon>(config::typeId, config::typeName, "召唤会追踪敌人、受伤和死亡的分身。", params,
                              9, 0, {160, 240, 255, 255});
    r->add(r->user, &t);
}
