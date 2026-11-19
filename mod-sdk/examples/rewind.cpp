#include "orbduel_abilities.hpp"
#include <deque>
#ifdef ORBDUEL_USER_MOD
#include "mod_config.hpp"
#else
namespace config {
constexpr char typeId[] = "rewind.recall", typeName[] = "时间回溯";
constexpr float lookback = 3, cooldown = 8, threshold = .45f;
} // namespace config
#endif
using namespace orbmod;
namespace {
const EntityApi *entities = nullptr;
struct Rewind : Ability {
    Rewind(const Host *h, Id id) : Ability(h, id, entities) {}
    struct Snapshot {
        float time;
        Vec pos, velocity;
        float hp;
    };
    std::deque<Snapshot> history;
    Cooldown cooldown;
    Interval sample;
    float flash = 0;
    Vec from{}, to{};
    void step(float dt) {
        beginStep();
        cooldown.tick(dt);
        flash = std::max(0.f, flash - dt);
        Entity self{};
        if (!owner(self))
            return;
        float now = host->time(host->user);
        float window = std::max(.1f, std::min(10.f, number("lookback", config::lookback)));
        if (history.empty() || sample.tick(dt, .1f))
            history.push_back({now, self.pos, self.velocity, self.hp});
        while (history.size() > 1 && history[1].time <= now - window)
            history.pop_front();
    }
    void event(const Event &e) {
        Entity self{};
        if (!hurt(e) || !cooldown.ready() || !owner(self) || history.empty() ||
            self.hp > self.maxHp * number("threshold", config::threshold))
            return;
        float cutoff =
            host->time(host->user) - std::max(.1f, std::min(10.f, number("lookback", config::lookback)));
        Snapshot saved = history.front();
        for (const auto &snapshot : history) {
            if (snapshot.time > cutoff)
                break;
            saved = snapshot;
        }
        from = self.pos;
        to = saved.pos;
        flash = .6f;
        host->setPosition(host->user, self.id, saved.pos);
        host->setVelocity(host->user, self.id, saved.velocity);
        float healing = std::max(0.f, saved.hp - self.hp);
        host->heal(host->user, self.id, healing);
        cooldown.start(number("cooldown", config::cooldown));
        history.clear();
        Event signal{"rewind.used", self.id, -1, id, saved.pos, healing, "ability"};
        host->emit(host->user, &signal);
    }
    void draw(const Draw &d) {
        auto o = object();
        d.circle(d.user, o.pos, 12, {140, 180, 255, 170}, 0);
        if (flash > 0) {
            d.line(d.user, from, to, {140, 180, 255, 180}, 2);
            d.circle(d.user, to, 35 * flash, {140, 180, 255, 180}, 0);
        }
    }
};
} // namespace
ORBDUEL_MOD_EXPORT void orbduel_set_entity_api(const EntityApi *api) {
    entities = api;
}
ORBDUEL_MOD_EXPORT void orbduel_register(Registrar *r) {
    static const Param params[] = {
        {"lookback", "回溯秒数", ParamKind::Number, config::lookback, .1f, 10, "",
         "历史不足时使用最早记录；不复活死亡实体"},
        {"cooldown", "冷却秒数", ParamKind::Number, config::cooldown, .1f, 30, "", "本能力实例独立计时"},
        {"threshold", "触发血量比例", ParamKind::Number, config::threshold, .05f, 1, "",
         "受到生命伤害且低于此比例时触发"}};
    auto t =
        behavior<Rewind>(config::typeId, config::typeName, "回到之前的位置和速度，并恢复此前较高的生命。",
                         params, 3, 0, {140, 180, 255, 255});
    r->add(r->user, &t);
}
