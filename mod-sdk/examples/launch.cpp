#include "orbduel_mod.hpp"
#include <cstring>
#include <unordered_map>
using namespace orbmod;
struct Launch : Behavior {
    using Behavior::Behavior;
    std::unordered_map<int, float> ready;
    void event(const Event &e) {
        if (std::strcmp(e.name, "enter") || e.object != id)
            return;
        float now = host->time(host->user);
        if (ready[e.a] > now)
            return;
        auto o = object();
        auto dir = direction(o.angle);
        float speed = number("speed", 700);
        host->setVelocity(host->user, e.a, {dir.x * speed, dir.y * speed});
        host->shield(host->user, e.a, number("shield", 0), 2);
        ready[e.a] = now + number("cooldown", .5f);
        Event signal{"launch.used", e.a, -1, id, o.pos, speed, "launch"};
        host->emit(host->user, &signal);
    }
    void draw(const Draw &d) {
        auto o = object();
        Color c{255, 190, 90, 220};
        d.circle(d.user, o.pos, o.radius, {255, 190, 90, 25}, 1);
        d.circle(d.user, o.pos, o.radius, c, 0);
        auto dir = direction(o.angle);
        Vec normal{-dir.y, dir.x};
        for (int i = 0; i < 3; ++i) {
            float offset = (i - 1) * 14.f;
            Vec p{o.pos.x + dir.x * offset, o.pos.y + dir.y * offset};
            d.line(d.user, {p.x - dir.x * 7 + normal.x * 9, p.y - dir.y * 7 + normal.y * 9}, p, c, 3);
            d.line(d.user, {p.x - dir.x * 7 - normal.x * 9, p.y - dir.y * 7 - normal.y * 9}, p, c, 3);
        }
    }
};
ORBDUEL_MOD_EXPORT void orbduel_register(Registrar *r) {
    static const Param params[] = {
        {"speed", "弹射速度", ParamKind::Number, 700, 0, 1800, "", "速度方向由对象朝向控制"},
        {"shield", "附带护盾", ParamKind::Number, 0, 0, 100, "", "弹射时同时获得两秒护盾"},
        {"cooldown", "使用间隔（秒）", ParamKind::Number, .5f, .1f, 10, "", ""}};
    auto t = behavior<Launch>("launch.pad", "弹射板", "进入区域后沿指定方向弹射，可附带护盾。", params, 3, 1,
                              {255, 190, 90, 255});
    r->add(r->user, &t);
}
