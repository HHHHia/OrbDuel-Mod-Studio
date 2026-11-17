#include "mod_config.hpp"
#include "orbduel_mod.hpp"
using namespace orbmod;
// 示例新机制：沿切线推动范围内的球，形成旋风；修改 strength 即可调参。
struct Vortex : Behavior {
    using Behavior::Behavior;
    void step(float) {
        auto o = object();
        for (int i = 0; i < count(); ++i) {
            auto e = entity(i);
            float dist = distance(e.pos, o.pos);
            if (!e.alive || dist < 5 || dist > o.radius)
                continue;
            float s = number("strength", config::strength);
            host->accelerate(host->user, e.id,
                             {-(e.pos.y - o.pos.y) / dist * s, (e.pos.x - o.pos.x) / dist * s});
        }
    }
    void draw(const Draw &d) {
        auto o = object();
        d.circle(d.user, o.pos, o.radius, {210, 130, 255, 130}, 0);
    }
};
ORBDUEL_MOD_EXPORT void orbduel_register(Registrar *r) {
    static const Param params[] = {
        {"strength", "旋转强度", ParamKind::Number, config::strength, -1200, 1200, "", "负数反向旋转"}};
    auto t = behavior<Vortex>(config::typeId, config::typeName, "通过切向加速度创造新的运动轨迹。", params, 1,
                              1, {210, 130, 255, 255});
    r->add(r->user, &t);
}
