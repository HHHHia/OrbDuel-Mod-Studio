#include "orbduel_mod.hpp"
using namespace orbmod;
struct Gravity : Behavior {
    using Behavior::Behavior;
    void step(float) {
        auto o = object();
        for (int i = 0; i < count(); ++i) {
            auto e = entity(i);
            if (!e.alive)
                continue;
            int filter = int(number("affect"));
            if (filter && e.id != filter - 1)
                continue;
            float dist = distance(e.pos, o.pos);
            if (dist < 5 || dist > o.radius)
                continue;
            float strength = number("strength", 180);
            if (number("repel"))
                strength = -strength;
            host->accelerate(host->user, e.id,
                             {(o.pos.x - e.pos.x) / dist * strength, (o.pos.y - e.pos.y) / dist * strength});
        }
    }
    void draw(const Draw &d) {
        auto o = object();
        Color c = number("repel") ? Color{255, 130, 175, 150} : Color{80, 220, 200, 150};
        d.circle(d.user, o.pos, o.radius, {c.r, c.g, c.b, 15}, 1);
        d.circle(d.user, o.pos, o.radius, c, 0);
        float time = host->time(host->user);
        for (int i = 0; i < 3; ++i) {
            float r = o.radius * (.2f + .22f * i) + std::sin(time * 2 + i) * 4;
            d.circle(d.user, o.pos, r, {c.r, c.g, c.b, 60}, 0);
        }
        d.circle(d.user, o.pos, 7, c, 1);
    }
};
ORBDUEL_MOD_EXPORT void orbduel_register(Registrar *r) {
    static const Param params[] = {
        {"strength", "加速度", ParamKind::Number, 180, 0, 1200, "", "每秒改变的速度；范围由对象半径决定"},
        {"repel", "向外排斥", ParamKind::Toggle, 0, 0, 1, "", "关闭时吸引，开启时排斥"},
        {"affect", "作用对象", ParamKind::Choice, 0, 0, 2, "所有球|左方球|右方球", ""}};
    auto t = behavior<Gravity>("gravity.field", "引力场", "在范围内持续吸引或排斥球，可随角色移动。", params,
                               3, 1, {80, 220, 200, 255});
    r->add(r->user, &t);
}
