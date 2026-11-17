#include "orbduel_mod.hpp"
using namespace orbmod;
// 熔岩池：范围内的球持续掉血。伤害按环境原因记（source = -1），不吃连击、也不触发反伤。
// 每 0.25 秒结算一次，而不是每步一次：战斗记录不会被 120 条/秒的伤害刷屏。
struct Lava : Behavior {
    using Behavior::Behavior;
    float bank = 0;

    void step(float dt) {
        bank += dt;
        if (bank < .25f)
            return;
        const float seconds = bank;
        bank = 0;
        auto o = object();
        float dps = number("dps", 8);
        if (dps <= 0)
            return;
        for (int i = 0; i < count(); ++i) {
            auto e = entity(i);
            if (!e.alive || distance(e.pos, o.pos) > o.radius)
                continue;
            host->damage(host->user, -1, e.id, dps * seconds, "environment");
        }
    }

    void draw(const Draw &d) {
        auto o = object();
        float time = host->time(host->user);
        d.circle(d.user, o.pos, o.radius, {255, 90, 40, 45}, 1);
        d.circle(d.user, o.pos, o.radius, {255, 130, 60, 190}, 0);
        for (int i = 0; i < 3; ++i) {
            float a = time * 1.1f + float(i) * 2.1f;
            float r = o.radius * (.35f + .2f * float(i));
            Vec p{o.pos.x + std::cos(a) * r, o.pos.y + std::sin(a) * r};
            d.circle(d.user, p, 4 + 2 * std::sin(time * 3 + float(i)), {255, 190, 110, 200}, 1);
        }
    }
};
ORBDUEL_MOD_EXPORT void orbduel_register(Registrar *r) {
    static const Param params[] = {
        {"dps", "每秒伤害", ParamKind::Number, 8, 0, 60, "", "范围内的球持续掉血；0 = 只有外观"}};
    auto t = behavior<Lava>("lava.pool", "熔岩池", "范围内的球持续掉血，伤害记为环境伤害。", params, 1, 1,
                            {255, 120, 60, 255});
    r->add(r->user, &t);
}
