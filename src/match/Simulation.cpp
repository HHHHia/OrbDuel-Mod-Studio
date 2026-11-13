#include "orbduel/match/Simulation.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace orbduel {
namespace {
Vec2 v(orbmod::Vec p) {
    return {p.x, p.y};
}
orbmod::Vec v(Vec2 p) {
    return {p.x, p.y};
}
} // namespace
SceneObject *Simulation::object(int id) {
    for (auto &o : scene.objects)
        if (o.id == id)
            return &o;
    return nullptr;
}
Simulation::Simulation(const SceneDocument &s, const ContentPack &p, const PluginManager &plugins,
                       bool preview)
    : scene(s), pack(p), world(events, arena), match(events, world, arena), rng(s.map.seed),
      plugins_(plugins), preview_(preview) {
    match.setup(scene.fighters[0], scene.fighters[1], scene.map, pack, rng);
    match.configure(s.timeLimit, s.shrink, s.stall);
    match.start();
    nextId_ = scene.nextId();
    host_.user = this;
    host_.entityCount = [](void *p) { return int(static_cast<Simulation *>(p)->world.bodies().size()); };
    host_.entityAt = [](void *p, int index, orbmod::Entity *out) {
        auto &s = *static_cast<Simulation *>(p);
        auto *b = s.world.body(index);
        if (!b || !out)
            return 0;
        const Fighter *f = nullptr;
        for (int i = 0; i < 2; ++i)
            if (s.match.fighter(i).bodyId == b->id)
                f = &s.match.fighter(i);
        *out = {b->id,         v(b->pos),        v(b->vel),         b->radius,
                f ? f->hp : 0, f ? f->maxHp : 0, f ? f->shield : 0, b->active ? 1 : 0};
        return 1;
    };
    host_.getObject = [](void *p, int id, orbmod::Object *out) {
        auto *o = static_cast<Simulation *>(p)->object(id);
        if (!o || !out)
            return 0;
        *out = {o->id, v(o->pos), v(o->size), o->radius, o->angle, o->owner};
        return 1;
    };
    host_.number = [](void *p, int id, const char *key, float fallback) {
        auto *o = static_cast<Simulation *>(p)->object(id);
        if (!o)
            return fallback;
        auto it = o->params.find(key);
        if (it == o->params.end())
            return fallback;
        if (it->is_boolean())
            return it->get<bool>() ? 1.f : 0.f;
        return it->is_number() ? it->get<float>() : fallback;
    };
    host_.text = [](void *p, int id, const char *key, char *out, int cap) {
        auto *o = static_cast<Simulation *>(p)->object(id);
        if (!o || !out || cap <= 0)
            return 0;
        std::string t = o->params.value(key, std::string{});
        auto n = std::min(int(t.size()), cap - 1);
        std::memcpy(out, t.data(), n);
        out[n] = 0;
        return n;
    };
    host_.setPosition = [](void *p, int id, orbmod::Vec pos) {
        auto &s = *static_cast<Simulation *>(p);
        if (auto *b = s.world.body(id)) {
            b->pos = v(pos);
            s.arena.resolveOverlaps(*b, 1.f);
        }
    };
    host_.setVelocity = [](void *p, int id, orbmod::Vec vel) {
        if (auto *b = static_cast<Simulation *>(p)->world.body(id))
            b->vel = v(vel);
    };
    host_.accelerate = [](void *p, int id, orbmod::Vec force) {
        if (auto *b = static_cast<Simulation *>(p)->world.body(id))
            b->force += v(force);
    };
    host_.damage = [](void *p, int from, int to, float amount, const char *cause) {
        static_cast<Simulation *>(p)->match.damage(from, to, amount, cause ? cause : "mod");
    };
    host_.heal = [](void *p, int id, float amount) { static_cast<Simulation *>(p)->match.heal(id, amount); };
    host_.shield = [](void *p, int id, float amount, float seconds) {
        static_cast<Simulation *>(p)->match.giveShield(id, amount, seconds);
    };
    host_.spawn = [](void *p, const char *type, orbmod::Vec pos, const char *params) {
        auto &s = *static_cast<Simulation *>(p);
        auto *t = s.plugins_.find(type);
        if (!t)
            return -1;
        SceneObject o;
        o.id = s.nextId_++;
        o.type = type;
        o.pos = v(pos);
        o.params = s.plugins_.defaults(*t);
        if (params) {
            auto j = nlohmann::json::parse(params, nullptr, false);
            if (j.is_object())
                o.params.update(j);
        }
        s.pendingSpawn_.push_back(o);
        return o.id;
    };
    host_.remove = [](void *p, int id) { static_cast<Simulation *>(p)->pendingRemove_.push_back(id); };
    host_.emit = [](void *p, const orbmod::Event *e) {
        auto &s = *static_cast<Simulation *>(p);
        if (e)
            s.pendingEvents_.push_back({e->name ? e->name : "custom", e->cause ? e->cause : "", e->a, e->b,
                                        e->object, v(e->pos), e->value});
    };
    host_.random = [](void *p, float lo, float hi) {
        return static_cast<Simulation *>(p)->rng.range(lo, hi);
    };
    host_.time = [](void *p) { return static_cast<Simulation *>(p)->match.elapsed(); };
    host_.color = [](void *p, int id, const char *key, orbmod::Color fallback) {
        auto *o = static_cast<Simulation *>(p)->object(id);
        if (!o)
            return fallback;
        auto it = o->params.find(key);
        if (it == o->params.end() || !it->is_array() || it->size() != 4)
            return fallback;
        orbmod::Color c{};
        unsigned char *values[] = {&c.r, &c.g, &c.b, &c.a};
        for (int i = 0; i < 4; ++i) {
            if (!(*it)[i].is_number())
                return fallback;
            *values[i] = static_cast<unsigned char>(clampf((*it)[i].get<float>(), 0, 1) * 255);
        }
        return c;
    };
    for (int i = 0; i < int(EventType::Count); ++i)
        events.subscribe(static_cast<EventType>(i), [this](const Event &e) {
            match.onEvent(e);
            onEvent(e);
        });
    for (const auto &o : scene.objects)
        instantiate(o);
}
Simulation::~Simulation() {
    for (auto &i : instances_)
        i.type->api.destroy(i.state);
}
void Simulation::instantiate(const SceneObject &o) {
    if (auto *t = plugins_.find(o.type)) {
        auto *state = t->api.create(&host_, o.id);
        instances_.push_back({o.id, t, state});
    }
}
void Simulation::forward(const orbmod::Event &e) {
    for (auto &i : instances_)
        if (i.type->api.event)
            i.type->api.event(i.state, &e);
}
void Simulation::onEvent(const Event &e) {
    const char *name = "unknown";
    switch (e.type) {
    case EventType::Hit:
        name = "hit";
        break;
    case EventType::WallBounce:
        name = "wall_bounce";
        break;
    case EventType::Damage:
        name = "damage";
        break;
    case EventType::Death:
        name = "death";
        break;
    case EventType::RoundEnd:
        name = "round_end";
        break;
    case EventType::SkillCast:
        name = "skill_cast";
        break;
    default:
        break;
    }
    orbmod::Event event{name, e.a, e.b, -1, v(e.pos), e.value, e.tag.c_str()};
    forward(event);
    if (e.type == EventType::Hit) {
        flashes.push_back({e.pos, .35f});
    }
    if (e.type == EventType::Hit || e.type == EventType::Damage || e.type == EventType::SkillCast) {
        log.push_back(std::to_string(int(match.elapsed())) + "s  " + name + "  " + e.tag + "  " +
                      std::to_string(int(e.value)));
        if (log.size() > 40)
            log.erase(log.begin());
    }
}
void Simulation::step(float dt) {
    if (preview_ || match.phase() != MatchPhase::Fighting)
        return;
    // 自定义事件延后一固定步，回调产生事件不会递归执行。
    auto pending = std::move(pendingEvents_);
    pendingEvents_.clear();
    for (const auto &e : pending) {
        orbmod::Event a{e.name.c_str(), e.a, e.b, e.object, v(e.pos), e.value, e.cause.c_str()};
        forward(a);
        ++modActivations;
        flashes.push_back({e.pos, .5f});
        log.push_back(std::to_string(int(match.elapsed())) + "s  " + e.name);
        if (log.size() > 40)
            log.erase(log.begin());
    }
    for (auto &i : instances_)
        if (i.type->api.step)
            i.type->api.step(i.state, dt);
    match.update(dt);
    world.step(dt);
    auto crushed = arena.update(dt);
    (void)crushed;
    for (auto &b : world.bodies())
        if (b.active && arena.resolveOverlaps(b, scene.map.physics.wallRestitution) && scene.shrink)
            match.damage(-1, b.id, scene.map.shrink.damagePerSec * dt, "environment");
    std::set<std::pair<int, int>> now;
    for (auto &o : scene.objects) {
        if (o.owner >= 0 && o.owner < 2)
            if (auto *b = world.body(match.fighter(o.owner).bodyId))
                o.pos = b->pos;
        for (const auto &b : world.bodies())
            if (b.active && lengthOf(b.pos - o.pos) < b.radius + o.radius) {
                const auto pair = std::make_pair(o.id, b.id);
                now.insert(pair);
                orbmod::Event e{
                    inside_.count(pair) ? "stay" : "enter", b.id, -1, o.id, v(b.pos), dt, "sensor"};
                forward(e);
            }
    }
    for (auto pair : inside_)
        if (!now.count(pair)) {
            orbmod::Event e{"leave", pair.second, -1, pair.first, {0, 0}, 0, "sensor"};
            forward(e);
        }
    inside_ = std::move(now);
    events.dispatch();
    match.finishStep();
    maxLull = std::max(maxLull, match.stallGuard().lullSeconds());
    if (match.phase() == MatchPhase::Finished)
        events.dispatch();
    for (int id : pendingRemove_) {
        for (auto it = instances_.begin(); it != instances_.end();)
            if (it->id == id) {
                it->type->api.destroy(it->state);
                it = instances_.erase(it);
            } else
                ++it;
        scene.objects.erase(std::remove_if(scene.objects.begin(), scene.objects.end(),
                                           [id](const SceneObject &o) { return o.id == id; }),
                            scene.objects.end());
    }
    pendingRemove_.clear();
    auto spawned = std::move(pendingSpawn_);
    pendingSpawn_.clear();
    for (const auto &o : spawned) {
        scene.objects.push_back(o);
        instantiate(o);
    }
    for (auto &f : flashes)
        f.second -= dt;
    flashes.erase(std::remove_if(flashes.begin(), flashes.end(), [](const auto &f) { return f.second <= 0; }),
                  flashes.end());
}
void Simulation::draw(const orbmod::Draw &d) {
    for (auto &i : instances_)
        if (i.type->api.draw) {
            drawingAssetRoot = i.type->assetRoot;
            i.type->api.draw(i.state, &d);
        }
}
} // namespace orbduel
