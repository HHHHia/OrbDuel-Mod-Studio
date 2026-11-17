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
const orbmod::EntityApi *Simulation::entityApi() {
    static const orbmod::EntityApi api{
        [](void *p, const orbmod::EntitySpec *spec) {
            return p && spec ? static_cast<Simulation *>(p)->createEntity(*spec) : -1;
        },
        [](void *p, int id) {
            if (!p)
                return;
            auto &s = *static_cast<Simulation *>(p);
            const auto *f = s.match.fighterById(id);
            if ((f && !f->primary) || (!f && s.entityKnown(id)))
                s.removingEntities_[id] = "destroyed";
        },
        [](void *p, int id, orbmod::Entity *out) {
            return p && static_cast<Simulation *>(p)->getEntity(id, out) ? 1 : 0;
        },
        [](void *p, int id, orbmod::CombatInfo *out) {
            const auto *f = p ? static_cast<Simulation *>(p)->match.fighterById(id) : nullptr;
            if (!f || !out)
                return 0;
            *out = {f->team, f->owner, f->primary ? 1 : 0, f->lifetime};
            return 1;
        },
        [](void *p, int id, const char *type, const char *params) {
            return p ? static_cast<Simulation *>(p)->bindAbility(id, type, params) : -1;
        },
        [](void *p, int id) {
            if (p)
                static_cast<Simulation *>(p)->pendingRemove_.push_back(id);
        }};
    return &api;
}
bool Simulation::getEntity(int id, orbmod::Entity *out) const {
    const auto *b = world.body(id);
    const auto *f = match.fighterById(id);
    if (!out || !b || !f)
        return false;
    *out = {id, v(b->pos), v(b->vel), b->radius, f->hp, f->maxHp, f->shield, f->alive ? 1 : 0};
    return true;
}
bool Simulation::entityKnown(int id) const {
    if (const auto *f = match.fighterById(id))
        return f->alive;
    for (const auto &e : pendingEntities_)
        if (e.id == id)
            return true;
    return false;
}
int Simulation::createEntity(const orbmod::EntitySpec &spec) {
    if (preview_ || match.phase() != MatchPhase::Fighting || (spec.owner >= 0 && !entityKnown(spec.owner)))
        return -1;
    const CharacterDef *def = nullptr;
    int team = spec.team;
    if (spec.owner >= 0) {
        if (const auto *f = match.fighterById(spec.owner)) {
            def = f->def;
            if (team < 0)
                team = f->team;
        } else
            for (const auto &e : pendingEntities_)
                if (e.id == spec.owner) {
                    def = &e.def;
                    if (team < 0)
                        team = e.team;
                }
    }
    if (spec.character && *spec.character)
        def = pack.findCharacter(spec.character);
    if (!def || !std::isfinite(spec.hpScale) || spec.hpScale <= 0 || !std::isfinite(spec.radiusScale) ||
        spec.radiusScale <= 0 || !std::isfinite(spec.attackScale) || spec.attackScale < 0 ||
        !std::isfinite(spec.lifetime) || spec.lifetime < 0 || !std::isfinite(spec.contact) ||
        spec.contact < 0 || !std::isfinite(spec.pos.x) || !std::isfinite(spec.pos.y) ||
        !std::isfinite(spec.velocity.x) || !std::isfinite(spec.velocity.y))
        return -1;
    CharacterDef copy = *def;
    copy.stats.maxHp *= spec.hpScale;
    copy.hitbox.radius *= spec.radiusScale;
    copy.stats.mass *= spec.radiusScale * spec.radiusScale;
    copy.stats.massMax *= spec.radiusScale * spec.radiusScale;
    copy.stats.attack *= spec.attackScale;
    copy.hitbox.contact = spec.contact;
    if (!std::isfinite(copy.stats.maxHp) || copy.stats.maxHp <= 0 || !std::isfinite(copy.hitbox.radius) ||
        copy.hitbox.radius <= 0 || !std::isfinite(copy.stats.mass) || !std::isfinite(copy.stats.massMax) ||
        !std::isfinite(copy.stats.attack))
        return -1;
    if (!spec.copySkills)
        copy.skills.clear();
    int id = world.reserveBodyId();
    pendingEntities_.push_back(
        {id, team, spec.owner, std::move(copy), v(spec.pos), v(spec.velocity), spec.lifetime});
    return id;
}
int Simulation::bindAbility(int entity, const char *type, const char *params) {
    const auto *t = type ? plugins_.find(type) : nullptr;
    if (!t || !entityKnown(entity) || preview_ || match.phase() != MatchPhase::Fighting)
        return -1;
    SceneObject o;
    o.id = nextId_++;
    o.type = type;
    o.owner = entity;
    o.params = plugins_.defaults(*t);
    if (params) {
        auto j = nlohmann::json::parse(params, nullptr, false);
        if (j.is_object())
            o.params.update(j);
    }
    pendingSpawn_.push_back(o);
    return o.id;
}
void Simulation::followOwners() {
    for (auto &o : scene.objects)
        if (auto *b = world.body(o.owner))
            o.pos = b->pos;
}
void Simulation::flushChanges() {
    // 快照队列：销毁或构造回调产生的新请求留到下一边界。
    auto creating = std::move(pendingEntities_);
    pendingEntities_.clear();
    auto spawning = std::move(pendingSpawn_);
    pendingSpawn_.clear();
    auto removed = std::move(pendingRemove_);
    pendingRemove_.clear();
    auto doomed = std::move(removingEntities_);
    removingEntities_.clear();
    const bool finished = match.phase() != MatchPhase::Fighting;
    for (int i = 2; i < match.fighterCount(); ++i) {
        const auto &f = match.fighter(i);
        if (finished)
            doomed[f.bodyId] = "round_end";
        else if (!f.alive)
            doomed[f.bodyId] = "death";
        else if (f.expires && f.lifetime <= 0)
            doomed[f.bodyId] = "lifetime";
    }
    // 所有者死亡、移除或尚未出生就取消，会递归清理后代。
    bool changed;
    do {
        changed = false;
        for (int i = 2; i < match.fighterCount(); ++i) {
            const auto &f = match.fighter(i);
            const auto *owner = match.fighterById(f.owner);
            if (f.owner >= 0 && (!owner || !owner->alive || doomed.count(f.owner)) &&
                !doomed.count(f.bodyId)) {
                doomed[f.bodyId] = "owner_removed";
                changed = true;
            }
        }
        for (const auto &e : creating)
            if (e.owner >= 0 && doomed.count(e.owner) && !doomed.count(e.id)) {
                doomed[e.id] = "owner_removed";
                changed = true;
            }
    } while (changed);
    for (auto it = doomed.begin(); it != doomed.end();)
        if (const auto *f = match.fighterById(it->first); f && f->primary)
            it = doomed.erase(it);
        else
            ++it;
    for (const auto &entry : doomed)
        if (const auto *f = match.fighterById(entry.first)) {
            Event e;
            e.type = EventType::EntityRemoved;
            e.a = f->bodyId;
            e.b = f->owner;
            e.tag = entry.second;
            if (const auto *b = world.body(f->bodyId))
                e.pos = b->pos;
            events.publish(e);
        }
    // 先送达 death / entity_removed，再销毁绑定能力；每实例只销毁一次。
    if (!doomed.empty())
        events.dispatch();
    for (const auto &o : scene.objects) {
        const auto *f = match.fighterById(o.owner);
        if (o.owner >= 0 && (finished || !f || !f->alive || doomed.count(o.owner)))
            removed.push_back(o.id);
    }
    for (int id : removed) {
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
    for (const auto &entry : doomed)
        match.removeCombatant(entry.first);
    if (!finished)
        for (const auto &e : creating) {
            const auto *owner = match.fighterById(e.owner);
            if (doomed.count(e.id) || (e.owner >= 0 && (!owner || !owner->alive)))
                continue;
            if (match.addCombatant(e.id, e.def, e.pos, e.velocity, e.team, e.owner, e.lifetime) >= 0) {
                Event event;
                event.type = EventType::EntitySpawned;
                event.a = e.id;
                event.b = e.owner;
                event.pos = e.pos;
                events.publish(event);
            }
        }
    if (!finished)
        for (auto &o : spawning) {
            const auto *f = match.fighterById(o.owner);
            if (std::find(removed.begin(), removed.end(), o.id) != removed.end() ||
                (o.owner >= 0 && (!f || !f->alive || doomed.count(o.owner))))
                continue;
            if (auto *b = world.body(o.owner))
                o.pos = b->pos;
            scene.objects.push_back(o);
            instantiate(o);
        }
    for (auto it = inside_.begin(); it != inside_.end();)
        if (!object(it->first) || !world.body(it->second))
            it = inside_.erase(it);
        else
            ++it;
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
        const auto &bodies = s.world.bodies();
        return index >= 0 && index < int(bodies.size()) && s.getEntity(bodies[index].id, out) ? 1 : 0;
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
        if (auto *b = s.world.body(id); b && b->active && std::isfinite(pos.x) && std::isfinite(pos.y)) {
            b->pos = v(pos);
            s.arena.resolveOverlaps(*b, 1.f);
        }
    };
    host_.setVelocity = [](void *p, int id, orbmod::Vec vel) {
        if (auto *b = static_cast<Simulation *>(p)->world.body(id);
            b && b->active && std::isfinite(vel.x) && std::isfinite(vel.y))
            b->vel = v(vel);
    };
    host_.accelerate = [](void *p, int id, orbmod::Vec force) {
        if (auto *b = static_cast<Simulation *>(p)->world.body(id);
            b && b->active && std::isfinite(force.x) && std::isfinite(force.y))
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
    case EventType::EntitySpawned:
        name = "entity_spawned";
        break;
    case EventType::EntityRemoved:
        name = "entity_removed";
        break;
    case EventType::SkillCast:
        name = "skill_cast";
        break;
    default:
        break;
    }
    orbmod::Event event{name, e.a, e.b, -1, v(e.pos), e.value, e.tag.c_str()};
    forward(event);
    if (e.type == EventType::Hit || e.type == EventType::SkillExpire) {
        flashes.push_back({e.pos, .5f});
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
    flushChanges();
    followOwners();
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
    arena.update(dt);
    // 场地已经装不下角色时先结算，避免退化边界继续参与物理。
    match.finishStep();
    if (match.phase() != MatchPhase::Fighting) {
        events.dispatch();
        flushChanges();
        return;
    }
    applyFields(dt);
    world.step(dt);
    for (auto &b : world.bodies())
        if (b.active && arena.resolveOverlaps(b, scene.map.physics.wallRestitution) && scene.shrink)
            match.damage(-1, b.id, scene.map.shrink.damagePerSec * dt, "environment");
    std::set<std::pair<int, int>> now;
    for (auto &o : scene.objects) {
        if (o.owner >= 0)
            if (auto *b = world.body(o.owner))
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
    flushChanges();
    for (auto &f : flashes)
        f.second -= dt;
    flashes.erase(std::remove_if(flashes.begin(), flashes.end(), [](const auto &f) { return f.second <= 0; }),
                  flashes.end());
}
void Simulation::applyFields(float dt) {
    if (scene.map.fields.empty())
        return;
    fieldClock_ += dt;
    const bool settle = fieldClock_ >= .25f;
    const float seconds = fieldClock_;
    for (const FieldDef &field : scene.map.fields) {
        for (int i = 0; i < match.fighterCount(); ++i) {
            if (field.affect != 0 && field.affect != match.fighter(i).team + 1)
                continue;
            Body *b = world.body(match.fighter(i).bodyId);
            if (b == nullptr || !b->active)
                continue;
            if (lengthOf(b->pos - field.pos) > field.radius)
                continue;
            if (field.accel.x != 0.0f || field.accel.y != 0.0f)
                b->force += field.accel;
            if (!settle)
                continue;
            if (field.dps > 0.0f)
                match.damage(-1, b->id, field.dps * seconds, "environment");
            if (field.heal > 0.0f)
                match.heal(b->id, field.heal * seconds);
        }
    }
    if (settle)
        fieldClock_ = 0.0f;
}
void Simulation::draw(const orbmod::Draw &d) {
    for (auto &i : instances_)
        if (i.type->api.draw) {
            drawingAssetRoot = i.type->assetRoot;
            i.type->api.draw(i.state, &d);
        }
}
} // namespace orbduel
