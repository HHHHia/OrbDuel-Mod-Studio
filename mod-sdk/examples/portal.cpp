#include "orbduel_mod.hpp"
#include <cstring>
#include <unordered_map>
using namespace orbmod;
struct Portal : Behavior {
    using Behavior::Behavior;
    std::unordered_map<int, float> ready;
    void event(const Event &e) {
        // 动态实体的编号不复用，离场后释放对应冷却，避免长局召唤持续积累条目。
        if (!std::strcmp(e.name, "entity_removed") || !std::strcmp(e.name, "death")) {
            ready.erase(e.a);
            return;
        }
        if (std::strcmp(e.name, "enter") || e.object != id)
            return;
        int filter = int(number("affect"));
        if (filter && e.a != filter - 1)
            return;
        float now = host->time(host->user);
        if (ready[e.a] > now)
            return;
        Object out{};
        if (!host->getObject(host->user, int(number("target", -1)), &out) || out.id == id)
            return;
        Entity ball{};
        bool found = false;
        for (int i = 0; i < count(); ++i) {
            auto b = entity(i);
            if (b.id == e.a) {
                ball = b;
                found = true;
                break;
            }
        }
        if (!found)
            return;
        auto dir = direction(out.angle);
        float speed = std::sqrt(ball.velocity.x * ball.velocity.x + ball.velocity.y * ball.velocity.y);
        Vec velocity = ball.velocity;
        float gain = number("gain", 1);
        if (number("mode") > 0)
            velocity = {dir.x * speed, dir.y * speed};
        host->setPosition(host->user, e.a,
                          {out.pos.x + dir.x * (out.radius + ball.radius + 8),
                           out.pos.y + dir.y * (out.radius + ball.radius + 8)});
        host->setVelocity(host->user, e.a, {velocity.x * gain, velocity.y * gain});
        ready[e.a] = now + number("cooldown", .7f);
        Event signal{"portal.used", e.a, -1, id, out.pos, 1, "portal"};
        host->emit(host->user, &signal);
    }
    void draw(const Draw &d) {
        auto o = object();
        Color c = color("color", {115, 145, 255, 255});
        d.circle(d.user, o.pos, o.radius + 5, {c.r, c.g, c.b, 24}, 1);
        d.circle(d.user, o.pos, o.radius, {c.r, c.g, c.b, 32}, 1);
        d.circle(d.user, o.pos, o.radius, c, 0);
        d.circle(d.user, o.pos, o.radius * .75f, {180, 150, 255, 140}, 0);
        auto dir = direction(o.angle);
        d.line(d.user, o.pos, {o.pos.x + dir.x * (o.radius + 16), o.pos.y + dir.y * (o.radius + 16)}, c, 2);
    }
};
ORBDUEL_MOD_EXPORT void orbduel_register(Registrar *r) {
    static const Param params[] = {
        {"target", "关联出口", ParamKind::ObjectRef, -1, -1, 10000, "", "选择另一个传送门作为出口"},
        {"mode", "出口速度", ParamKind::Choice, 1, 0, 1, "保留原方向|按出口朝向", ""},
        {"gain", "速度倍率", ParamKind::Number, 1, .1f, 4, "", "通过后速度的倍率"},
        {"cooldown", "使用间隔（秒）", ParamKind::Number, .7f, .1f, 10, "", "同一球再次使用此入口的间隔"},
        {"affect", "允许使用", ParamKind::Choice, 0, 0, 2, "所有球|左方球|右方球", ""},
        {"color", "传送门颜色", ParamKind::Color, 0, 0, 1, "", "仅改变外观，不影响物理"}};
    auto t = behavior<Portal>("portal.portal", "传送门", "关联两个入口，改变球的位置与出口速度。", params, 6,
                              2, {130, 150, 255, 255});
    r->add(r->user, &t);
}
